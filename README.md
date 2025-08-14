# 🌐 C Dynamic & Multi-Threaded HTTP Server (v2.0)

A **high-performance, multi-threaded HTTP/1.1 server** written from scratch in C, designed for learning, experimentation, and real-world web service deployment. This project demonstrates **advanced networking, concurrency**, and **web protocol handling**, serving both static and dynamic content with robust routing and request processing.

Built with **POSIX Sockets** and **Pthreads**, it provides a solid base for understanding network programming, HTTP protocol internals, and scalable server architectures.

---

## 📚 Table of Contents

- [Key Features](#key-features)
- [Project Structure](#project-structure)
- [Requirements](#requirements)
- [Installation & Building](#installation--building)
- [Running the Server](#running-the-server)
- [Technical Architecture](#technical-architecture)
- [HTTP Request Lifecycle](#http-request-lifecycle)
- [Testing With cURL](#testing-with-curl)
- [Makefile Utilities](#makefile-utilities)
- [Optimizations & Scalability](#optimizations--scalability)
- [Extending the Server](#extending-the-server)
- [Troubleshooting](#troubleshooting)
- [Learning Resources](#learning-resources)
- [License](#license)
- [Contributing](#contributing)
- [FAQ](#faq)

---

## ✨ Key Features

### Core

- **Multi-Threaded Architecture**  
  Uses POSIX threads (`pthread`) for concurrent request handling, allowing hundreds of simultaneous connections without blocking.
- **Static File Serving**  
  Efficiently serves HTML, CSS, JS, images, and other files from the `www/` root directory.
- **Automatic MIME Type Detection**  
  Maps file extensions to the correct `Content-Type` header for proper browser rendering.
- **Robust Logging**  
  Maintains `access.log` for request tracking and `error.log` for server faults, each with timestamped entries.

### Dynamic Capabilities

- **Dynamic Routing Engine**  
  Internal routing table maps URL paths to C handler functions for flexible, application-specific logic.
- **HTTP Method Support**  
  - `GET`: Retrieve files or dynamic data.
  - `POST`: Submit and process forms or data payloads.
  - `HEAD`: Fetch headers only, for bandwidth-efficient health checks.
- **Built-In Endpoints**
  - `/time`: Returns the current server time as HTML.
  - `/status`: Displays real-time server statistics (uptime, request count).
  - `/echo`: Handles `POST` form submissions and echoes user data.
- **Server Statistics**  
  Tracks uptime and total requests, using thread-safe counters.
- **Form Data Parsing**  
  Parses `application/x-www-form-urlencoded` POST bodies into key-value pairs.

---

## 📂 Project Structure

```
jitacm-30_days_c_-c-server/
│
├── README.md              # Documentation
│
└── C-Server/
    ├── Http_server.c      # Main server source code
    ├── Makefile           # Build/test/clean automation
    └── www/               # Web root for static content
        ├── index.html     # Homepage
        ├── 404.html       # Custom 404 error page
        └── style.css      # Stylesheet
```

---

## ⚙️ Requirements

- **POSIX-compliant OS** (Linux, macOS, WSL)
- **GCC** (C99 or higher)
- **Make**
- **cURL** (for endpoint testing)

Check installations:
```bash
gcc --version
make --version
curl --version
```

---

## 📥 Installation & Building

1. **Clone the Repository**
    ```bash
    git clone https://github.com/yourusername/c-server.git
    cd c-server/C-Server
    ```

2. **Build the Project**
    ```bash
    make
    ```
    Produces the `server` executable; links pthread automatically.

3. **Directory Structure**
    - `www/` contains all files served statically.
    - Edit `index.html`/`404.html`/`style.css` to customize site appearance.

---

## ▶️ Running the Server

**Default port (8080):**
```bash
./server
```
**Custom port (e.g., 5000):**
```bash
./server 5000
```

**Startup Output Example:**
```
Server running on port 8080...
Available endpoints:
  - http://localhost:8080/ (Homepage)
  - http://localhost:8080/time (Server time)
  - http://localhost:8080/status (Server status)
  - http://localhost:8080/echo (Form demo)
Press Ctrl+C to stop the server.
```

Browse to `http://localhost:8080` to view the homepage.

---

## 🛠 Technical Architecture

- **Language:** C (C99 standard)
- **Networking:** POSIX sockets (`socket`, `bind`, `listen`, `accept`)
- **Concurrency:** POSIX threads (`pthread_create`, `pthread_detach`)
- **Synchronization:** Mutexes for logging and server statistics
- **Routing:** Static and dynamic routes mapped to handler functions
- **HTTP Protocol:** Full HTTP/1.1 compliance for requests and responses

---

## 🌐 HTTP Request Lifecycle

1. **Connection Accept:**  
   Main thread listens and accepts incoming TCP connections.
2. **Thread Spawn:**  
   Each connection is handled in a new, detached thread.
3. **Request Parsing:**  
   Thread parses the HTTP request line, headers, and body.
4. **Routing Decision:**  
   - If path matches a dynamic endpoint (e.g., `/time`), runs corresponding handler.
   - Else, treats as request for a static file in `www/`.
5. **Static File Handling:**  
   - Checks file existence and permissions.
   - Streams file contents (with correct MIME type) or serves custom `404.html`.
6. **Dynamic Handler Execution:**  
   - Generates HTML response or JSON/XML as needed.
   - Handles form data and custom logic.
7. **Response Formation:**  
   - Assembles HTTP headers and body based on request method (`GET`, `HEAD`, `POST`).
8. **Logging:**  
   - Writes to `access.log` and/or `error.log` with timestamps and details.
9. **Thread Exit:**  
   - Closes connection and terminates thread.

**Thread safety** is ensured for shared counters, logs, and server status via mutexes.

---

## 🧪 Testing With cURL

**1. GET request (dynamic endpoint):**
```bash
curl http://localhost:8080/status
```

**2. HEAD request:**
```bash
curl -I http://localhost:8080/time
```

**3. POST request (form data):**
```bash
curl -X POST -d "name=Alice&message=Hello from cURL" http://localhost:8080/echo
```

**4. Run Makefile's test suite:**
```bash
make test
```
Automates endpoint tests and reports results.

---

## 🧹 Makefile Utilities

- **Build:**  
  `make` — compiles `Http_server.c` and links pthread.
- **Test:**  
  `make test` — runs a suite of cURL checks against endpoints.
- **Clean:**  
  `make clean` — removes binaries and log files.

---

## 🚀 Optimizations & Scalability

- **Thread Pooling (future):**  
  Current version spawns/detaches threads per connection; a thread pool can further optimize performance under heavy load.
- **Non-blocking I/O:**  
  For maximum scalability, integrate `select`, `poll`, or `epoll` (Linux) for event-driven architecture.
- **Persistent Connections:**  
  HTTP/1.1 keep-alive support for faster repeat requests.
- **Configurable Logging Levels:**  
  Switch between verbose debugging and silent production modes via config.

---

## 🔧 Extending the Server

- **Add Endpoints:**  
  Extend the routing table in `Http_server.c` with new URL paths and C handler functions.
- **Serve Other MIME Types:**  
  Expand MIME mapping for PDFs, videos, or custom formats.
- **Implement HTTPS:**  
  Use OpenSSL for SSL/TLS support on top of sockets.
- **Add REST API:**  
  Return JSON for API endpoints and support client-side JS applications.
- **Session Management:**  
  Implement cookies and session tracking for login systems.
- **Rate Limiting & Security:**  
  Protect against abuse, add IP blacklisting, and sanitize user input.

---

## 🩺 Troubleshooting

- **Server Won't Start:**  
  Ensure port is free, and you have sufficient permissions.
- **Can't Access Endpoints:**  
  Check firewall or SELinux restrictions.
- **Log Files Not Written:**  
  Ensure write permissions for the current directory.
- **cURL Errors:**  
  Use `curl -v` for verbose error messages.

---

## 📘 Learning Resources

- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [HTTP/1.1 RFC](https://datatracker.ietf.org/doc/html/rfc2616)
- [POSIX Threads Programming](https://computing.llnl.gov/tutorials/pthreads/)
- [MIME Types List](https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types)
- [Advanced C Programming](https://www.cprogramming.com/tutorial/c-tutorial.html)

---

## 🏷️ License

MIT License — see [LICENSE](LICENSE) for details.

---

## 🤝 Contributing

We welcome pull requests for bug fixes, new features, documentation improvements, and code refactoring!  
Please fork the repository, create a feature branch, and submit a detailed PR.

- Follow C99 style and comment your code.
- Add tests for new endpoints.
- Update documentation for new features.

---

## ❓ FAQ

**Q: Can I run this server on Windows?**  
A: Use WSL (Windows Subsystem for Linux) for full POSIX support.

**Q: How do I add a new route?**  
A: Edit the routing table in `Http_server.c`, add a handler function, and map the path to the function.

**Q: Does it support HTTPS?**  
A: Not yet; see "Extending the Server" for OpenSSL integration.

**Q: What's the maximum number of connections?**  
A: Limited by system resources (RAM, CPU) and OS file descriptor limits.

**Q: How do I change the web root directory?**  
A: Edit the path in `Http_server.c` (`www/` by default).

**Q: Can I serve large files?**  
A: Yes, but streaming/partial content support is recommended for files larger than RAM.

---

Thank you for using, studying, and contributing to the C Dynamic & Multi-Threaded HTTP Server! 🚀
