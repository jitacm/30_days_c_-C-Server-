C-Server: A Simple HTTP Server
--------------------
A lightweight, single-threaded HTTP/1.0 server written in pure C from scratch. This project is a deep dive into low-level network programming with sockets, parsing the HTTP protocol, and handling file I/O in a Linux/macOS environment. The server is capable of serving static files (HTML, CSS, images) from a designated web root directory.

Features
---------------
Pure C Implementation: Built with standard C libraries, with no external dependencies.

HTTP/1.0 GET Requests: Parses and handles basic GET requests for static files.

Static File Serving: Serves any file type from the ./www web root directory.

MIME Type Handling: Automatically determines the Content-Type for common web files (.html, .css, .js, .jpg, etc.).

Error Handling: Responds with a 404 Not Found page for requests to non-existent files.

Single-Threaded & Blocking: Uses a simple, single-threaded, blocking I/O model to handle one request at a time.

Getting Started
-----------------
To get the server running on your local machine, you will need a C compiler (like GCC) and the make utility, which are standard on most Linux and macOS systems.

1. Clone the Repository
First, clone this repository to your local machine.

git clone <your-repository-url>
cd <repository-directory>

2. Compile the Project
A Makefile is included for easy compilation. Simply run the make command.

make

This will compile server.c and create an executable file named server in the root directory.

3. Run the Server
Launch the server and specify a port number. The standard port is 8080.

./server 8080

The server is now running and listening for connections on http://localhost:8080.

4. Test the Server
Homepage: Open your web browser and navigate to http://localhost:8080. You should see the index.html page.

404 Page: Try to access a file that doesn't exist, like http://localhost:8080/nonexistent.html, to see the custom 404 error page.

Project Structure
The project has a simple and clear file structure.

.
├── Makefile          # The build script for compiling the server.
├── server            # The compiled executable (created by make).
├── server.c          # The complete C source code for the server.
└── www/              # The web root directory containing all static files.
    ├── 404.html      # The page served for 404 errors.
    ├── index.html    # The main homepage.
    └── style.css     # The stylesheet for the HTML pages.

How It Works
-----------------
The server follows a classic network programming workflow:

Socket Creation: Creates a TCP socket using socket().

Binding: Binds the socket to an IP address and port number using bind().

Listening: Puts the server socket in a listening state using listen(), ready to accept incoming connections.

Accepting: Enters an infinite loop, blocking on accept() until a client connects.

Request Handling:

Once a client connects, it reads the incoming HTTP request into a buffer.

It parses the request line to extract the URI (e.g., /index.html).

It constructs the full file path relative to the ./www directory.

Response Generation:

If the requested file exists, it constructs and sends a 200 OK HTTP response header, including the correct Content-Type and Content-Length.

If the file does not exist, it sends a 404 Not Found response.

It then reads the file's content and sends it as the response body.

Closing: The connection to the client is closed, and the server loops back to accept() to wait for the next connection.

License
-------------
this is open source 