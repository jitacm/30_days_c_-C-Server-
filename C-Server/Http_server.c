#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>

#define DEFAULT_PORT 8080
#define DEFAULT_WEBROOT "./www"
#define BUFFER_SIZE 4096

int server_socket;  // For graceful shutdown

// Function to handle Ctrl+C (graceful shutdown)
void handle_sigint(int sig) {
    printf("\nShutting down server...\n");
    close(server_socket);
    exit(0);
}

// Function to get MIME type of a file based on its extension
const char* get_mime_type(const char* path) {
    const char *dot = strrchr(path, '.');
    if (dot) {
        if (strcmp(dot, ".html") == 0) return "text/html";
        if (strcmp(dot, ".css") == 0) return "text/css";
        if (strcmp(dot, ".js") == 0) return "application/javascript";
        if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) return "image/jpeg";
        if (strcmp(dot, ".png") == 0) return "image/png";
        if (strcmp(dot, ".gif") == 0) return "image/gif";
    }
    return "application/octet-stream";
}

// Sends an HTTP error response (e.g., 404 Not Found)
void send_error_response(int client_socket, int status_code, const char* status_message, const char* file_path) {
    char response_header[BUFFER_SIZE];
    char file_buffer[BUFFER_SIZE];
    int file_fd = open(file_path, O_RDONLY);
    ssize_t bytes_read = 0;

    if (file_fd != -1) {
        bytes_read = read(file_fd, file_buffer, BUFFER_SIZE - 1);
        file_buffer[bytes_read] = '\0';
        close(file_fd);
    } else {
        snprintf(file_buffer, BUFFER_SIZE, "<h1>%d %s</h1>", status_code, status_message);
        bytes_read = strlen(file_buffer);
    }

    snprintf(response_header, sizeof(response_header),
             "HTTP/1.0 %d %s\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "Server: Simple-C-Server\r\n"
             "\r\n",
             status_code, status_message, bytes_read);

    send(client_socket, response_header, strlen(response_header), 0);
    send(client_socket, file_buffer, bytes_read, 0);
}

// Sends a successful HTTP response with the requested file
void send_file_response(int client_socket, const char* file_path) {
    char response_header[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];

    int file_fd = open(file_path, O_RDONLY);
    if (file_fd == -1) {
        send_error_response(client_socket, 404, "Not Found", "./www/404.html");
        return;
    }

    // Get file size
    off_t file_size = lseek(file_fd, 0, SEEK_END);
    lseek(file_fd, 0, SEEK_SET);

    const char* mime_type = get_mime_type(file_path);

    snprintf(response_header, sizeof(response_header),
             "HTTP/1.0 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "Server: Simple-C-Server\r\n"
             "\r\n",
             mime_type, file_size);

    send(client_socket, response_header, strlen(response_header), 0);

    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    close(file_fd);
}

// Handles a single client connection
void handle_client(int client_socket, const char* webroot) {
    char buffer[BUFFER_SIZE];
    char file_path[BUFFER_SIZE];

    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        close(client_socket);
        return;
    }
    buffer[bytes_received] = '\0';

    char method[16], uri[256];
    sscanf(buffer, "%s %s", method, uri);

    printf("[LOG] Request: %s %s\n", method, uri);

    if (strcmp(method, "GET") != 0) {
        send_error_response(client_socket, 405, "Method Not Allowed", "./www/404.html");
        close(client_socket);
        return;
    }

    if (strstr(uri, "..")) {
        send_error_response(client_socket, 400, "Bad Request", "./www/404.html");
        close(client_socket);
        return;
    }

    if (strcmp(uri, "/") == 0) {
        strcpy(uri, "/index.html");
    }

    snprintf(file_path, sizeof(file_path), "%s%s", webroot, uri);
    send_file_response(client_socket, file_path);

    close(client_socket);
}

int main(int argc, char *argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : DEFAULT_PORT;
    const char* webroot = (argc > 2) ? argv[2] : DEFAULT_WEBROOT;

    signal(SIGINT, handle_sigint);

    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server running on http://localhost:%d\n", port);
    printf("Serving files from: %s\n", webroot);

    while (1) {
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("accept");
            continue;
        }

        printf("[INFO] Connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        handle_client(client_socket, webroot);
    }

    close(server_socket);
    return 0;
}
