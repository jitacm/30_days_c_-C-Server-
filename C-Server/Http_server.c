#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define WEBROOT "./www"
#define LOG_FILE "access.log"
#define ERROR_LOG_FILE "error.log"

// HTTP status codes
#define HTTP_OK 200
#define HTTP_NOT_FOUND 404
#define HTTP_METHOD_NOT_ALLOWED 405
#define HTTP_INTERNAL_SERVER_ERROR 500

// Structure for HTTP request data
typedef struct {
    char method[8];
    char path[256];
    char version[16];
} HttpRequest;

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
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    if (strcmp(ext, ".mp4") == 0) return "video/mp4";

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
    time_str[strlen(time_str)-1] = '\0'; // remove newline

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
    sscanf(request_str, "%s %s %s", request->method, request->path, request->version);
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

    snprintf(header, sizeof(header),
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n",
             status_text, mime_type, content_length);

    send(client_socket, header, strlen(header), 0);
}

/**
 * Sends a file to the client
 */
void send_file(int client_socket, const char *path, const char *client_ip, const char *method) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        const char *not_found = "<h1>404 Not Found</h1>";
        send_response_header(client_socket, HTTP_NOT_FOUND, "text/html", strlen(not_found));
        send(client_socket, not_found, strlen(not_found), 0);
        log_request(client_ip, method, path, HTTP_NOT_FOUND);
        log_error("File not found");
        return;
    }

    struct stat st;
    stat(path, &st);
    const char *mime_type = get_mime_type(path);

    send_response_header(client_socket, HTTP_OK, mime_type, st.st_size);

    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    fclose(file);
    log_request(client_ip, method, path, HTTP_OK);
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

        // Only allow GET method for now
        if (strcmp(request.method, "GET") != 0) {
            const char *method_not_allowed = "<h1>405 Method Not Allowed</h1>";
            send_response_header(client_socket, HTTP_METHOD_NOT_ALLOWED, "text/html", strlen(method_not_allowed));
            send(client_socket, method_not_allowed, strlen(method_not_allowed), 0);
            log_request(client_ip, request.method, request.path, HTTP_METHOD_NOT_ALLOWED);
            close(client_socket);
            return NULL;
        }

        // If root requested, serve index.html
        if (strcmp(request.path, "/") == 0) {
            strcpy(request.path, "/index.html");
        }

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s%s", WEBROOT, request.path);
        send_file(client_socket, full_path, client_ip, request.method);
    }

    close(client_socket);
    return NULL;
}

/**
 * Main entry point
 */
int main() {
    int server_fd, *client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

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

    printf("Server running on port %d...\n", PORT);

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
