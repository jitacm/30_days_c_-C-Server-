#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <ctype.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_HEADERS 50
#define WEBROOT "./www"
#define LOG_FILE "access.log"
#define ERROR_LOG_FILE "error.log"

// HTTP status codes
#define HTTP_OK 200
#define HTTP_NOT_FOUND 404
#define HTTP_METHOD_NOT_ALLOWED 405
#define HTTP_INTERNAL_SERVER_ERROR 500

// Server statistics
typedef struct {
    time_t start_time;
    unsigned long request_count;
    unsigned long bytes_sent;
    pthread_mutex_t mutex;
} ServerStats;

// Global server statistics
ServerStats server_stats = {0, 0, 0, PTHREAD_MUTEX_INITIALIZER};

// HTTP Header structure
typedef struct {
    char name[128];
    char value[512];
} HttpHeader;

// Structure for HTTP request data
typedef struct {
    char method[8];
    char path[256];
    char version[16];
    HttpHeader headers[MAX_HEADERS];
    int header_count;
    char *body;
    size_t body_length;
} HttpRequest;

// Route handler function type
typedef void (*RouteHandler)(int client_socket, HttpRequest *request, const char *client_ip);

// Route structure
typedef struct {
    char path[256];
    char methods[32];  // Comma-separated list of allowed methods
    RouteHandler handler;
} Route;

// Forward declarations for route handlers
void handle_time(int client_socket, HttpRequest *request, const char *client_ip);
void handle_status(int client_socket, HttpRequest *request, const char *client_ip);
void handle_echo_form(int client_socket, HttpRequest *request, const char *client_ip);
void handle_static_file(int client_socket, HttpRequest *request, const char *client_ip);

// Dynamic routing table
Route routes[] = {
    {"/time", "GET,HEAD", handle_time},
    {"/status", "GET,HEAD", handle_status},
    {"/echo", "GET,POST,HEAD", handle_echo_form},
    {"", "", NULL}  // Default route (must be last)
};

/**
 * Updates server statistics
 */
void update_stats(unsigned long bytes) {
    pthread_mutex_lock(&server_stats.mutex);
    server_stats.request_count++;
    server_stats.bytes_sent += bytes;
    pthread_mutex_unlock(&server_stats.mutex);
}

/**
 * URL decode function for POST data
 */
void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if (*src == '%' && ((a = src[1]) && (b = src[2])) && 
            (isxdigit(a) && isxdigit(b))) {
            if (a >= 'a') a -= 'a' - 'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a' - 'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16 * a + b;
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

/**
 * Parse form data from POST request
 */
void parse_form_data(const char *data, char *key, char *value, size_t max_len) {
    char *amp = strchr(data, '&');
    char *eq = strchr(data, '=');
    
    if (eq) {
        size_t key_len = eq - data;
        if (key_len >= max_len) key_len = max_len - 1;
        strncpy(key, data, key_len);
        key[key_len] = '\0';
        
        const char *val_start = eq + 1;
        size_t val_len = amp ? (size_t)(amp - val_start) : strlen(val_start);
        if (val_len >= max_len) val_len = max_len - 1;
        strncpy(value, val_start, val_len);
        value[val_len] = '\0';
        
        url_decode(value, value);
    }
}

/**
 * MIME type detection
 */
const char* get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";

    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".htm") == 0) return "text/html";
    if (strcmp(ext, ".css") == 0) return "text/css";
    if (strcmp(ext, ".js") == 0) return "application/javascript";
    if (strcmp(ext, ".json") == 0) return "application/json";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".gif") == 0) return "image/gif";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcmp(ext, ".ico") == 0) return "image/x-icon";
    if (strcmp(ext, ".txt") == 0) return "text/plain";
    if (strcmp(ext, ".pdf") == 0) return "application/pdf";
    if (strcmp(ext, ".zip") == 0) return "application/zip";

    return "application/octet-stream";
}

/**
 * Logs an access request
 */
void log_request(const char *client_ip, const char *method, const char *path, int status_code) {
    FILE *log_file = fopen(LOG_FILE, "a");
    if (!log_file) return;

    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str)-1] = '\0';

    fprintf(log_file, "[%s] %s \"%s %s\" %d\n", time_str, client_ip, method, path, status_code);
    fclose(log_file);
}

/**
 * Logs an error message
 */
void log_error(const char *message) {
    FILE *error_file = fopen(ERROR_LOG_FILE, "a");
    if (!error_file) return;

    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str)-1] = '\0';

    fprintf(error_file, "[%s] ERROR: %s\n", time_str, message);
    fclose(error_file);
}

/**
 * Parses an HTTP request string
 */
void parse_http_request(const char *request_str, HttpRequest *request) {
    // Initialize request structure
    memset(request, 0, sizeof(HttpRequest));
    
    // Parse request line
    char *line_end = strstr(request_str, "\r\n");
    if (!line_end) return;
    
    sscanf(request_str, "%s %s %s", request->method, request->path, request->version);
    
    // Parse headers
    const char *header_start = line_end + 2;
    request->header_count = 0;
    
    while (header_start && *header_start != '\r' && request->header_count < MAX_HEADERS) {
        line_end = strstr(header_start, "\r\n");
        if (!line_end) break;
        
        char *colon = strchr(header_start, ':');
        if (colon && colon < line_end) {
            size_t name_len = colon - header_start;
            if (name_len >= sizeof(request->headers[0].name)) {
                name_len = sizeof(request->headers[0].name) - 1;
            }
            
            strncpy(request->headers[request->header_count].name, header_start, name_len);
            request->headers[request->header_count].name[name_len] = '\0';
            
            // Skip colon and whitespace
            const char *value_start = colon + 1;
            while (*value_start == ' ' || *value_start == '\t') value_start++;
            
            size_t value_len = line_end - value_start;
            if (value_len >= sizeof(request->headers[0].value)) {
                value_len = sizeof(request->headers[0].value) - 1;
            }
            
            strncpy(request->headers[request->header_count].value, value_start, value_len);
            request->headers[request->header_count].value[value_len] = '\0';
            
            request->header_count++;
        }
        
        header_start = line_end + 2;
    }
    
    // Find body (after empty line)
    const char *body_start = strstr(request_str, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        request->body_length = strlen(body_start);
        if (request->body_length > 0) {
            request->body = malloc(request->body_length + 1);
            strcpy(request->body, body_start);
        }
    }
}

/**
 * Gets a header value from the request
 */
const char* get_header_value(HttpRequest *request, const char *name) {
    for (int i = 0; i < request->header_count; i++) {
        if (strcasecmp(request->headers[i].name, name) == 0) {
            return request->headers[i].value;
        }
    }
    return NULL;
}

/**
 * Sends an HTTP response header
 */
void send_response_header(int client_socket, int status_code, const char *mime_type, size_t content_length) {
    char header[BUFFER_SIZE];
    const char *status_text;

    switch (status_code) {
        case HTTP_OK: status_text = "200 OK"; break;
        case HTTP_NOT_FOUND: status_text = "404 Not Found"; break;
        case HTTP_METHOD_NOT_ALLOWED: status_text = "405 Method Not Allowed"; break;
        case HTTP_INTERNAL_SERVER_ERROR: status_text = "500 Internal Server Error"; break;
        default: status_text = "500 Internal Server Error"; break;
    }

    time_t now = time(NULL);
    char date_str[128];
    strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));

    snprintf(header, sizeof(header),
             "HTTP/1.1 %s\r\n"
             "Date: %s\r\n"
             "Server: C-HTTP-Server/2.0\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n",
             status_text, date_str, mime_type, content_length);

    send(client_socket, header, strlen(header), 0);
}

/**
 * Route handler for /time endpoint
 */
void handle_time(int client_socket, HttpRequest *request, const char *client_ip) {
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    char response[BUFFER_SIZE];
    
    snprintf(response, sizeof(response),
             "<!DOCTYPE html>"
             "<html>"
             "<head>"
             "<title>Server Time</title>"
             "<link rel=\"stylesheet\" href=\"/style.css\">"
             "</head>"
             "<body>"
             "<div class=\"container\">"
             "<h1>Current Server Time</h1>"
             "<p class=\"time\">%s</p>"
             "<p>Timezone: %s</p>"
             "<a href=\"/\">Back to Home</a>"
             "</div>"
             "</body>"
             "</html>",
             time_str, tzname[0]);
    
    send_response_header(client_socket, HTTP_OK, "text/html", strlen(response));
    
    // Send body only if not HEAD request
    if (strcmp(request->method, "HEAD") != 0) {
        send(client_socket, response, strlen(response), 0);
    }
    
    update_stats(strlen(response));
    log_request(client_ip, request->method, request->path, HTTP_OK);
}

/**
 * Route handler for /status endpoint
 */
void handle_status(int client_socket, HttpRequest *request, const char *client_ip) {
    time_t uptime = time(NULL) - server_stats.start_time;
    int hours = uptime / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = uptime % 60;
    
    char response[BUFFER_SIZE];
    
    pthread_mutex_lock(&server_stats.mutex);
    snprintf(response, sizeof(response),
             "<!DOCTYPE html>"
             "<html>"
             "<head>"
             "<title>Server Status</title>"
             "<link rel=\"stylesheet\" href=\"/style.css\">"
             "</head>"
             "<body>"
             "<div class=\"container\">"
             "<h1>Server Status</h1>"
             "<table style=\"margin: 0 auto; text-align: left;\">"
             "<tr><td><strong>Uptime:</strong></td><td>%d hours, %d minutes, %d seconds</td></tr>"
             "<tr><td><strong>Total Requests:</strong></td><td>%lu</td></tr>"
             "<tr><td><strong>Bytes Sent:</strong></td><td>%lu</td></tr>"
             "<tr><td><strong>Server Version:</strong></td><td>C-HTTP-Server/2.0</td></tr>"
             "<tr><td><strong>Port:</strong></td><td>%d</td></tr>"
             "</table>"
             "<a href=\"/\">Back to Home</a>"
             "</div>"
             "</body>"
             "</html>",
             hours, minutes, seconds,
             server_stats.request_count,
             server_stats.bytes_sent,
             PORT);
    pthread_mutex_unlock(&server_stats.mutex);
    
    send_response_header(client_socket, HTTP_OK, "text/html", strlen(response));
    
    if (strcmp(request->method, "HEAD") != 0) {
        send(client_socket, response, strlen(response), 0);
    }
    
    update_stats(strlen(response));
    log_request(client_ip, request->method, request->path, HTTP_OK);
}

/**
 * Route handler for /echo endpoint (demonstrates POST handling)
 */
void handle_echo_form(int client_socket, HttpRequest *request, const char *client_ip) {
    char response[BUFFER_SIZE];
    
    if (strcmp(request->method, "POST") == 0 && request->body) {
        // Parse POST data
        char name[256] = "";
        char message[512] = "";
        
        // Simple form parsing (expects name=value&name=value format)
        char *name_start = strstr(request->body, "name=");
        char *message_start = strstr(request->body, "message=");
        
        if (name_start) {
            parse_form_data(name_start, name, name, sizeof(name));
        }
        if (message_start) {
            parse_form_data(message_start, message, message, sizeof(message));
        }
        
        snprintf(response, sizeof(response),
                 "<!DOCTYPE html>"
                 "<html>"
                 "<head>"
                 "<title>Echo Response</title>"
                 "<link rel=\"stylesheet\" href=\"/style.css\">"
                 "</head>"
                 "<body>"
                 "<div class=\"container\">"
                 "<h1>Echo Response</h1>"
                 "<p><strong>Name:</strong> %s</p>"
                 "<p><strong>Message:</strong> %s</p>"
                 "<a href=\"/echo\">Submit Another</a> | "
                 "<a href=\"/\">Home</a>"
                 "</div>"
                 "</body>"
                 "</html>",
                 name[0] ? name : "(not provided)",
                 message[0] ? message : "(not provided)");
    } else {
        // Show form for GET request
        snprintf(response, sizeof(response),
                 "<!DOCTYPE html>"
                 "<html>"
                 "<head>"
                 "<title>Echo Form</title>"
                 "<link rel=\"stylesheet\" href=\"/style.css\">"
                 "</head>"
                 "<body>"
                 "<div class=\"container\">"
                 "<h1>Echo Form</h1>"
                 "<form method=\"POST\" action=\"/echo\">"
                 "<label>Name: <input type=\"text\" name=\"name\" required></label><br><br>"
                 "<label>Message: <textarea name=\"message\" rows=\"4\" cols=\"40\" required></textarea></label><br><br>"
                 "<input type=\"submit\" value=\"Submit\">"
                 "</form>"
                 "<a href=\"/\">Back to Home</a>"
                 "</div>"
                 "</body>"
                 "</html>");
    }
    
    send_response_header(client_socket, HTTP_OK, "text/html", strlen(response));
    
    if (strcmp(request->method, "HEAD") != 0) {
        send(client_socket, response, strlen(response), 0);
    }
    
    update_stats(strlen(response));
    log_request(client_ip, request->method, request->path, HTTP_OK);
}

/**
 * Sends a static file to the client
 */
void handle_static_file(int client_socket, HttpRequest *request, const char *client_ip) {
    char full_path[512];
    
    // If root requested, serve index.html
    if (strcmp(request->path, "/") == 0) {
        snprintf(full_path, sizeof(full_path), "%s/index.html", WEBROOT);
    } else {
        snprintf(full_path, sizeof(full_path), "%s%s", WEBROOT, request->path);
    }
    
    // Check if file exists
    struct stat st;
    if (stat(full_path, &st) != 0) {
        // Try to serve 404.html
        snprintf(full_path, sizeof(full_path), "%s/404.html", WEBROOT);
        if (stat(full_path, &st) != 0) {
            const char *not_found = "<h1>404 Not Found</h1>";
            send_response_header(client_socket, HTTP_NOT_FOUND, "text/html", strlen(not_found));
            if (strcmp(request->method, "HEAD") != 0) {
                send(client_socket, not_found, strlen(not_found), 0);
            }
            log_request(client_ip, request->method, request->path, HTTP_NOT_FOUND);
            return;
        }
    }
    
    const char *mime_type = get_mime_type(full_path);
    send_response_header(client_socket, HTTP_OK, mime_type, st.st_size);
    
    // Send body only if not HEAD request
    if (strcmp(request->method, "HEAD") != 0) {
        FILE *file = fopen(full_path, "rb");
        if (file) {
            char buffer[BUFFER_SIZE];
            size_t bytes_read;
            while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
                send(client_socket, buffer, bytes_read, 0);
            }
            fclose(file);
        }
    }
    
    update_stats(st.st_size);
    log_request(client_ip, request->method, request->path, HTTP_OK);
}

/**
 * Check if method is allowed for route
 */
int is_method_allowed(const char *methods, const char *method) {
    return strstr(methods, method) != NULL;
}

/**
 * Find matching route
 */
Route* find_route(const char *path, const char *method) {
    // Check exact matches first
    for (int i = 0; routes[i].handler != NULL; i++) {
        if (strcmp(routes[i].path, path) == 0) {
            if (is_method_allowed(routes[i].methods, method)) {
                return &routes[i];
            }
            return NULL; // Path matches but method not allowed
        }
    }
    
    // Default route (static file handler)
    return NULL;
}

/**
 * Thread function to handle each client
 */
void* handle_client(void *arg) {
    int client_socket = *(int*)arg;
    free(arg);

    struct sockaddr_in client_addr;
    socklen_t addr_size = sizeof(client_addr);
    getpeername(client_socket, (struct sockaddr*)&client_addr, &addr_size);
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';

        HttpRequest request;
        parse_http_request(buffer, &request);

        // Check for supported methods
        if (strcmp(request.method, "GET") != 0 && 
            strcmp(request.method, "POST") != 0 && 
            strcmp(request.method, "HEAD") != 0) {
            const char *method_not_allowed = "<h1>405 Method Not Allowed</h1>";
            send_response_header(client_socket, HTTP_METHOD_NOT_ALLOWED, "text/html", strlen(method_not_allowed));
            send(client_socket, method_not_allowed, strlen(method_not_allowed), 0);
            log_request(client_ip, request.method, request.path, HTTP_METHOD_NOT_ALLOWED);
        } else {
            // Find matching route
            Route *route = find_route(request.path, request.method);
            
            if (route && route->handler) {
                // Dynamic route found
                route->handler(client_socket, &request, client_ip);
            } else if (!route) {
                // Try static file serving for GET/HEAD
                if (strcmp(request.method, "GET") == 0 || strcmp(request.method, "HEAD") == 0) {
                    handle_static_file(client_socket, &request, client_ip);
                } else {
                    const char *method_not_allowed = "<h1>405 Method Not Allowed</h1>";
                    send_response_header(client_socket, HTTP_METHOD_NOT_ALLOWED, "text/html", strlen(method_not_allowed));
                    send(client_socket, method_not_allowed, strlen(method_not_allowed), 0);
                    log_request(client_ip, request.method, request.path, HTTP_METHOD_NOT_ALLOWED);
                }
            } else {
                // Route exists but method not allowed
                const char *method_not_allowed = "<h1>405 Method Not Allowed</h1>";
                send_response_header(client_socket, HTTP_METHOD_NOT_ALLOWED, "text/html", strlen(method_not_allowed));
                send(client_socket, method_not_allowed, strlen(method_not_allowed), 0);
                log_request(client_ip, request.method, request.path, HTTP_METHOD_NOT_ALLOWED);
            }
        }
        
        // Clean up request
        if (request.body) {
            free(request.body);
        }
    }

    close(client_socket);
    return NULL;
}

/**
 * Signal handler for graceful shutdown
 */
void handle_signal(int sig) {
    if (sig == SIGINT) {
        printf("\nShutting down server...\n");
        exit(0);
    }
}

/**
 * Main entry point
 */
int main(int argc, char *argv[]) {
    int port = PORT;
    
    // Parse command line arguments
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number. Using default: %d\n", PORT);
            port = PORT;
        }
    }
    
    // Initialize server statistics
    server_stats.start_time = time(NULL);
    server_stats.request_count = 0;
    server_stats.bytes_sent = 0;
    
    // Set up signal handler
    signal(SIGINT, handle_signal);
    
    int server_fd, *client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Allow socket reuse
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server running on port %d...\n", port);
    printf("Available endpoints:\n");
    printf("  - http://localhost:%d/ (Homepage)\n", port);
    printf("  - http://localhost:%d/time (Server time)\n", port);
    printf("  - http://localhost:%d/status (Server status)\n", port);
    printf("  - http://localhost:%d/echo (Form demo)\n", port);
    printf("\nPress Ctrl+C to stop the server.\n\n");

    while (1) {
        client_socket = malloc(sizeof(int));
        *client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (*client_socket < 0) {
            perror("Accept failed");
            free(client_socket);
            continue;
        }

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, client_socket);
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}
