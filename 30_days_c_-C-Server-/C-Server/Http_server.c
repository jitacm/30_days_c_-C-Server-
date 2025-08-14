// server.c
// Build: gcc server.c -o server -pthread
// Usage: ./server [port] [webroot]
// Example: ./server 8080 ./www
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <inttypes.h>
#include <pthread.h>
#include <limits.h>
#include <dirent.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>


#define DEFAULT_PORT 8080
#define DEFAULT_WEBROOT "./www"
#define BUFFER_SIZE 8192

int server_socket = -1;
volatile int running = 1;

// Stats
pthread_mutex_t stats_lock = PTHREAD_MUTEX_INITIALIZER;
long total_requests = 0;
long active_connections = 0;
time_t server_start_time;

// Logging
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
const char *LOGFILE = "server.log";

// Helper to get current timestamp as string
static void now_str(char *buf, size_t n) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(buf, n, "%Y-%m-%d %H:%M:%S", &tm);
}

// Graceful shutdown
void handle_sigint(int sig) {
    (void)sig;
    running = 0;
    if (server_socket != -1) close(server_socket);
    printf("\nShutting down server...\n");
}

// Logging (thread-safe)
void write_log(const char *fmt, ...) {
    pthread_mutex_lock(&log_lock);
    FILE *f = fopen(LOGFILE, "a");
    if (!f) {
        perror("fopen logfile");
        pthread_mutex_unlock(&log_lock);
        return;
    }
    char ts[64];
    now_str(ts, sizeof(ts));
    fprintf(f, "[%s] ", ts);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);

    fprintf(f, "\n");
    fclose(f);
    pthread_mutex_unlock(&log_lock);
}

// MIME type detection
const char* get_mime_type(const char* path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (strcmp(dot, ".html") == 0) return "text/html";
    if (strcmp(dot, ".htm")  == 0) return "text/html";
    if (strcmp(dot, ".css")  == 0) return "text/css";
    if (strcmp(dot, ".js")   == 0) return "application/javascript";
    if (strcmp(dot, ".json") == 0) return "application/json";
    if (strcmp(dot, ".svg")  == 0) return "image/svg+xml";
    if (strcmp(dot, ".txt")  == 0) return "text/plain";
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(dot, ".png") == 0) return "image/png";
    if (strcmp(dot, ".gif") == 0) return "image/gif";
    if (strcmp(dot, ".ico") == 0) return "image/x-icon";
    if (strcmp(dot, ".woff2") == 0) return "font/woff2";
    if (strcmp(dot, ".mp4") == 0) return "video/mp4";
    return "application/octet-stream";
}

// URL-decode simple (%20 -> space). Not full RFC, but enough for file names without special chars.
void urldecode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            char hex[3] = {a, b, 0};
            *dst++ = (char) strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

// Send 404 or other error using optional file fallback
void send_error_response(int client_socket, int status_code, const char* status_message, const char* webroot) {
    char header[BUFFER_SIZE];
    char body[BUFFER_SIZE];

    // try to open custom error page webroot/404.html
    char errpath[PATH_MAX];
    snprintf(errpath, sizeof(errpath), "%s/404.html", webroot);
    int fd = open(errpath, O_RDONLY);
    ssize_t r = 0;
    if (fd != -1) {
        r = read(fd, body, sizeof(body)-1);
        if (r < 0) r = 0;
        body[r] = '\0';
        close(fd);
    } else {
        snprintf(body, sizeof(body), "<html><head><title>%d %s</title></head><body style='font-family:sans-serif;padding:30px;'><h1>%d %s</h1><p>Sorry, an error occurred.</p></body></html>", status_code, status_message, status_code, status_message);
        r = strlen(body);
    }

    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: text/html; charset=utf-8\r\n"
             "Content-Length: %zd\r\n"
             "Connection: close\r\n"
             "Server: Simple-C-Server/1.1\r\n"
             "\r\n",
             status_code, status_message, r);

    send(client_socket, header, strlen(header), 0);
    send(client_socket, body, r, 0);
}

// Send file contents
void send_file_response(int client_socket, const char* file_path) {
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        // Caller should handle 404
        return;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        return;
    }
    off_t filesize = st.st_size;
    const char *mime = get_mime_type(file_path);

    char header[BUFFER_SIZE];
    int header_len = snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %jd\r\n"
             "Connection: close\r\n"
             "Server: Simple-C-Server/1.1\r\n"
             "\r\n",
             mime, (intmax_t)filesize);

    send(client_socket, header, header_len, 0);

    ssize_t bytes;
    char buf[4096];
    while ((bytes = read(fd, buf, sizeof(buf))) > 0) {
        ssize_t s = send(client_socket, buf, bytes, 0);
        if (s <= 0) break;
    }
    close(fd);
}

// Send generated directory listing as HTML
void send_directory_listing(int client_socket, const char* dirpath, const char* uri) {
    DIR *d = opendir(dirpath);
    if (!d) {
        // 403 or 404
        send_error_response(client_socket, 404, "Not Found", ".");
        return;
    }

    char header[BUFFER_SIZE];
    char body[BUFFER_SIZE*4];
    size_t offset = 0;
    offset += snprintf(body + offset, sizeof(body) - offset,
        "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Index of %s</title>"
        "<style>body{font-family:Segoe UI,Roboto,Arial;background:#0D1117;color:#c9d1d9;padding:20px}a{color:#58a6ff}</style></head><body><h1>Index of %s</h1><ul>",
        uri, uri);

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        // skip . and ..
        if (strcmp(ent->d_name, ".") == 0) continue;
        char name_esc[PATH_MAX];
        // naive escape: only replace spaces for display
        for (size_t i=0; i<strlen(ent->d_name) && i < sizeof(name_esc)-1; ++i) name_esc[i] = ent->d_name[i];
        name_esc[strlen(ent->d_name)] = '\0';

        // build link: uri + "/" + ent->d_name (careful with trailing slash)
        char link[PATH_MAX];
        if (uri[strlen(uri)-1] == '/') snprintf(link, sizeof(link), "%s%s", uri, ent->d_name);
        else snprintf(link, sizeof(link), "%s/%s", uri, ent->d_name);

        offset += snprintf(body + offset, sizeof(body) - offset,
                           "<li><a href=\"%s\">%s</a></li>",
                           link, name_esc);

        if (offset > sizeof(body) - 200) break; // prevent overflow
    }
    closedir(d);

    offset += snprintf(body + offset, sizeof(body) - offset, "</ul><hr><a href=\"/\">Home</a></body></html>");

    int header_len = snprintf(header, sizeof(header),
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/html; charset=utf-8\r\n"
              "Content-Length: %zu\r\n"
              "Connection: close\r\n"
              "Server: Simple-C-Server/1.1\r\n"
              "\r\n",
              strlen(body));

    send(client_socket, header, header_len, 0);
    send(client_socket, body, strlen(body), 0);
}

// Serve /status endpoint with simple stats page
void send_status_page(int client_socket) {
    char body[BUFFER_SIZE];
    time_t now = time(NULL);
    time_t uptime = now - server_start_time;

    pthread_mutex_lock(&stats_lock);
    long req = total_requests;
    long active = active_connections;
    pthread_mutex_unlock(&stats_lock);

    char upstr[128];
    int days = uptime / 86400;
    int hours = (uptime % 86400) / 3600;
    int mins = (uptime % 3600) / 60;
    int secs = uptime % 60;
    snprintf(upstr, sizeof(upstr), "%dd %dh %dm %ds", days, hours, mins, secs);

    int blen = snprintf(body, sizeof(body),
        "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Server Status</title>"
        "<style>body{font-family:Segoe UI,Roboto,Arial;background:#0D1117;color:#c9d1d9;padding:20px} .card{background:#161b22;padding:20px;border-radius:8px;border:1px solid #30363d;max-width:700px} h1{color:#58a6ff}</style></head><body><div class='card'><h1>Server Status</h1>"
        "<p><strong>Uptime:</strong> %s</p>"
        "<p><strong>Total requests:</strong> %ld</p>"
        "<p><strong>Active connections:</strong> %ld</p>"
        "<p><a href='/'>Home</a></p></div></body></html>",
        upstr, req, active);

    char header[BUFFER_SIZE];
    int header_len = snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html; charset=utf-8\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "Server: Simple-C-Server/1.1\r\n"
             "\r\n",
             blen);

    send(client_socket, header, header_len, 0);
    send(client_socket, body, blen, 0);
}

// Thread worker argument
typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
    char webroot[PATH_MAX];
} worker_arg_t;
void *worker_thread(void *arg) {
    worker_arg_t *w = (worker_arg_t*)arg;
    int client_socket = w->client_socket;
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(w->client_addr.sin_addr), client_ip, sizeof(client_ip));
    int client_port = ntohs(w->client_addr.sin_port);

    // increment active connections
    pthread_mutex_lock(&stats_lock);
    active_connections++;
    total_requests++;
    pthread_mutex_unlock(&stats_lock);

    // Read request (simple)
    char buf[BUFFER_SIZE];
    ssize_t bytes = recv(client_socket, buf, sizeof(buf)-1, 0);
    if (bytes <= 0) {
        close(client_socket);
        pthread_mutex_lock(&stats_lock); active_connections--; pthread_mutex_unlock(&stats_lock);
        free(w);
        return NULL;
    }
    buf[bytes] = '\0';

    // parse first line
    char method[16], uri[1024], protocol[32];
    if (sscanf(buf, "%15s %1023s %31s", method, uri, protocol) != 3) {
        send_error_response(client_socket, 400, "Bad Request", w->webroot);
        close(client_socket);
        pthread_mutex_lock(&stats_lock); active_connections--; pthread_mutex_unlock(&stats_lock);
        free(w);
        return NULL;
    }

    // log to console and file
    printf("[INFO] %s:%d -> %s %s\n", client_ip, client_port, method, uri);
    write_log("%s:%d %s %s", client_ip, client_port, method, uri);

    // only support GET
    if (strcmp(method, "GET") != 0) {
        send_error_response(client_socket, 405, "Method Not Allowed", w->webroot);
        close(client_socket);
        pthread_mutex_lock(&stats_lock); active_connections--; pthread_mutex_unlock(&stats_lock);
        free(w);
        return NULL;
    }

    // security: disallow parent traversal
    if (strstr(uri, "..")) {
        send_error_response(client_socket, 400, "Bad Request", w->webroot);
        close(client_socket);
        pthread_mutex_lock(&stats_lock); active_connections--; pthread_mutex_unlock(&stats_lock);
        free(w);
        return NULL;
    }

    // decode URL into path (decoded buffer is PATH_MAX)
    char decoded[PATH_MAX];
    urldecode(decoded, uri);

    // handle /status endpoint
    if (strcmp(decoded, "/status") == 0) {
        send_status_page(client_socket);
        close(client_socket);
        pthread_mutex_lock(&stats_lock); active_connections--; pthread_mutex_unlock(&stats_lock);
        free(w);
        return NULL;
    }

    // map "/" to "/index.html"
    if (strcmp(decoded, "/") == 0) strcpy(decoded, "/index.html");

    // --- Build fullpath safely using dynamic allocation ---
    size_t len_webroot = strlen(w->webroot);
    size_t len_decoded = strlen(decoded);
    size_t need_full = len_webroot + len_decoded + 1; // +1 for NUL

    char *fullpath = malloc(need_full);
    if (!fullpath) {
        send_error_response(client_socket, 500, "Internal Server Error", w->webroot);
        close(client_socket);
        pthread_mutex_lock(&stats_lock); active_connections--; pthread_mutex_unlock(&stats_lock);
        free(w);
        return NULL;
    }
    // Compose fullpath
    memcpy(fullpath, w->webroot, len_webroot);
    memcpy(fullpath + len_webroot, decoded, len_decoded);
    fullpath[need_full - 1] = '\0';

    struct stat st;
    if (stat(fullpath, &st) == -1) {
        // try directory with trailing slash
        if (len_decoded == 0) {
            send_error_response(client_socket, 404, "Not Found", w->webroot);
            goto cleanup_and_finish;
        }

        // Build maybe_dir = webroot + decoded + (maybe trailing '/')
        int add_slash = (decoded[len_decoded - 1] != '/');
        size_t need_maybe = len_webroot + len_decoded + (add_slash ? 1 : 0) + 1;
        char *maybe_dir = malloc(need_maybe);
        if (!maybe_dir) {
            send_error_response(client_socket, 500, "Internal Server Error", w->webroot);
            goto cleanup_and_finish;
        }
        memcpy(maybe_dir, w->webroot, len_webroot);
        memcpy(maybe_dir + len_webroot, decoded, len_decoded);
        if (add_slash) maybe_dir[len_webroot + len_decoded] = '/';
        maybe_dir[need_maybe - 1] = '\0';

        if (stat(maybe_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
            // directory exists -> list it
            send_directory_listing(client_socket, maybe_dir, decoded);
            free(maybe_dir);
            goto cleanup_and_finish;
        }

        free(maybe_dir);
        // not found
        send_error_response(client_socket, 404, "Not Found", w->webroot);
        goto cleanup_and_finish;
    }

    if (S_ISDIR(st.st_mode)) {
        // try index.html inside the directory
        const char *index_suffix = "/index.html";
        size_t need_index = strlen(fullpath) + strlen(index_suffix) + 1;
        char *indexpath = malloc(need_index);
        if (!indexpath) {
            send_error_response(client_socket, 500, "Internal Server Error", w->webroot);
            goto cleanup_and_finish;
        }
        // ensure no double slash: fullpath may or may not end with '/'
        if (fullpath[strlen(fullpath)-1] == '/')
            snprintf(indexpath, need_index, "%sindex.html", fullpath);
        else
            snprintf(indexpath, need_index, "%s/index.html", fullpath);

        if (stat(indexpath, &st) == 0 && S_ISREG(st.st_mode)) {
            send_file_response(client_socket, indexpath);
            free(indexpath);
            goto cleanup_and_finish;
        }
        free(indexpath);

        // create uri_for_list (decoded with trailing slash)
        size_t need_uri_for_list = len_decoded + 2; // decoded + maybe '/' + NUL
        char *uri_for_list = malloc(need_uri_for_list);
        if (!uri_for_list) {
            send_error_response(client_socket, 500, "Internal Server Error", w->webroot);
            goto cleanup_and_finish;
        }
        if (decoded[len_decoded - 1] == '/')
            snprintf(uri_for_list, need_uri_for_list, "%s", decoded);
        else
            snprintf(uri_for_list, need_uri_for_list, "%s/", decoded);

        send_directory_listing(client_socket, fullpath, uri_for_list);
        free(uri_for_list);
        goto cleanup_and_finish;
    }

    // If file, send it
    send_file_response(client_socket, fullpath);

cleanup_and_finish:
    close(client_socket);
    pthread_mutex_lock(&stats_lock); active_connections--; pthread_mutex_unlock(&stats_lock);
    free(fullpath);
    free(w);
    return NULL;
}

int main(int argc, char *argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    const char *webroot = (argc > 2) ? argv[2] : DEFAULT_WEBROOT;

    server_start_time = time(NULL);

    // Signal
    signal(SIGINT, handle_sigint);

    // Create listening socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 128) < 0) {
        perror("listen");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server running on http://localhost:%d\n", port);
    printf("Serving files from: %s\n", webroot);
    write_log("Server started on port %d, webroot=%s", port, webroot);

    while (running) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addrlen);
        if (client_socket < 0) {
            if (errno == EINTR) break; // interrupted by signal
            perror("accept");
            continue;
        }

        // allocate worker arg
        worker_arg_t *w = calloc(1, sizeof(worker_arg_t));
        if (!w) {
            close(client_socket);
            continue;
        }
        w->client_socket = client_socket;
        w->client_addr = client_addr;
        strncpy(w->webroot, webroot, sizeof(w->webroot)-1);

        pthread_t tid;
        int rc = pthread_create(&tid, NULL, worker_thread, w);
        if (rc != 0) {
            perror("pthread_create");
            close(client_socket);
            free(w);
            continue;
        }
        pthread_detach(tid);
    }

    if (server_socket != -1) close(server_socket);
    printf("Server stopped.\n");
    write_log("Server stopped.");
    return 0;
}
