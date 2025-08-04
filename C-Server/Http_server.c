#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define WEBROOT "./www"

// Function to get the MIME type of a file based on its extension
const char* get_mime_type(const char* path) {
    const char *dot = strrchr(path, '.');
    if (dot) {
        if (strcmp(dot, ".html") == 0) return "text/html";
        if (strcmp(dot, ".css") == 0) return "text/css";
        if (strcmp(dot, ".js") == 0) return "application/javascript";
        if (strcmp(dot, ".jpg") == 0) return "image/jpeg";
        if (strcmp(dot, ".jpeg") == 0) return "image/jpeg";
        if (strcmp(dot, ".png") == 0) return "image/png";
        if (strcmp(dot, ".gif") == 0) return "image/gif";
    }
    return "application/octet-stream";
}

// Sends an HTTP error response (e.g., 404 Not Found)
void send_error_response(int client_socket, int status_code, const char* status_message, const char* file_path) {
    char response_buffer[BUFFER_SIZE];
    char file_buffer[BUFFER_SIZE];
    int file_fd = open(file_path, O_RDONLY);
    ssize_t bytes_read = 0;
    
    if (file_fd != -1) {
        bytes_read = read(file_fd, file_buffer, BUFFER_SIZE - 1);
        file_buffer[bytes_read] = '\0';
        close(file_fd);
    } else {
        // Fallback if the 404.html file itself is not found
        snprintf(file_buffer, BUFFER_SIZE, "<h1>%d %s</h1>", status_code, status_message);
        bytes_read = strlen(file_buffer);
    }

    snprintf(response_buffer, BUFFER_SIZE,
             "HTTP/1.0 %d %s\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %ld\r\n"
             "\r\n%s",
             status_code, status_message, bytes_read, file_buffer);

    send(client_socket, response_buffer, strlen(response_buffer), 0);
}


// Sends a successful HTTP response with the requested file
void send_file_response(int client_socket, const char* file_path) {
    char response_header[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    
    int file_fd = open(file_path, O_RDONLY);
    if (file_fd == -1) {
        // If file cannot be opened, send a 404 error
        send_error_response(client_socket, 404, "Not Found", WEBROOT "/404.html");
        return;
    }

    // Get file size
    off_t file_size = lseek(file_fd, 0, SEEK_END);
    lseek(file_fd, 0, SEEK_SET);

    // Get MIME type
    const char* mime_type = get_mime_type(file_path);

    // Construct and send the header
    snprintf(response_header, BUFFER_SIZE,
             "HTTP/1.0 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "\r\n",
             mime_type, file_size);
    send(client_socket, response_header, strlen(response_header), 0);

    // Send the file content in chunks
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    close(file_fd);
}

// Handles a single client connection
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    char file_path[BUFFER_SIZE];

    // Read the request from the client
    ssize_t bytes_received = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        close(client_socket);
        return;
    }
    buffer[bytes_received] = '\0';
    printf("Received Request:\n%s\n", buffer);

    // Parse the HTTP request to get the file path
    // Example: "GET /index.html HTTP/1.0"
    char method[16], uri[256];
    sscanf(buffer, "%s %s", method, uri);

    // For security, prevent directory traversal attacks
    if (strstr(uri, "..") != NULL) {
        send_error_response(client_socket, 400, "Bad Request", WEBROOT "/404.html");
        close(client_socket);
        return;
    }

    // If root is requested, serve index.html
    if (strcmp(uri, "/") == 0) {
        strcpy(uri, "/index.html");
    }

    // Construct the full file path
    snprintf(file_path, sizeof(file_path), "%s%s", WEBROOT, uri);

    // Send the appropriate response
    send_file_response(client_socket, file_path);

    close(client_socket);
}

int main(int argc, char *argv[]) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int port = PORT;

    if (argc > 1) {
        port = atoi(argv[1]);
    }

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set up server address struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket to address and port
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 10) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", port);
    printf("Serving files from %s directory.\n", WEBROOT);

    // Main server loop
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("accept");
            continue; // Continue to next iteration on accept error
        }
        printf("Accepted connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        
        // Handle client in a separate function (for this single-threaded server)
        handle_client(client_socket);
    }

    close(server_socket);
    return 0;
}

