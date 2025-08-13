# C Multi-Threaded HTTP Server

A robust and high-performance HTTP server implemented in **C**, designed to serve static files from the `www/` directory. The server leverages **multi-threading** to efficiently handle numerous concurrent client requests. It also includes a lightweight logging system and serves custom error pages for better user experience.

---

## 📂 Project Structure

```
README.md              # Project documentation
C-Server/
│
├── Makefile               # Build automation script
├── Http_server.c          # Main HTTP server source code
│
└── www/                   # Static content served by the server
    ├── index.html          # Homepage
    ├── 404.html            # Custom 404 error page
    ├── style.css           # Styling for the HTML pages
```

---

## ⚙️ Requirements

To compile and run this project, ensure you have the following installed:

* **GCC** or any C99-compliant compiler
* **Make** utility
* **POSIX-compliant environment** (Linux, macOS, or WSL on Windows)

Verify installations:

```bash
gcc --version
make --version
```

---

## 📥 Installation

1. **Clone the Repository**

```bash
git clone https://github.com/yourusername/c-server.git
cd c-server
```

2. **Build the Project**

```bash
make
```

This will produce an executable named `server` in the project root.

---

## ▶️ Running the Server

**Default port (8080)**:

```bash
./server
```

**Custom port**:

```bash
./server 5000
```

Once running, open your web browser and visit:

```
http://localhost:8080
```

The server will continue running until stopped manually with `Ctrl + C`.

---

## 🌐 Serving Static Files

Any file placed inside the `www/` directory will be accessible over HTTP. Example:

```
www/
├── index.html
├── style.css
```

Access them directly in your browser:

```
http://localhost:8080/index.html
http://localhost:8080/style.css
```

The server automatically detects MIME types and serves files with the appropriate content type.

---

## 📝 Logging System

The server prints detailed logs to the terminal for:

* Incoming client connections
* Requested file paths
* HTTP response codes (200 OK, 404 Not Found, etc.)
* Thread creation and cleanup events

Log persistence to files can be added in later development phases.

---

## 🛠 Technical Overview

* **Language**: C (C99)
* **Networking**: POSIX Sockets API
* **Concurrency**: POSIX Threads (`pthread`)
* **HTTP Method Supported**: GET
* **HTTP Version**: 1.1

**Request Handling Workflow**:

1. Server binds to the specified port and listens for incoming connections.
2. For each request, a new thread is spawned to handle it concurrently.
3. The requested resource is searched inside the `www/` directory.
4. If found, it is served with the correct headers; otherwise, a `404.html` page is returned.

---

## 🧹 Cleaning Up

To remove all compiled binaries and start fresh:

```bash
make clean
```

This ensures your workspace remains tidy and prevents outdated builds from causing issues.

---

## 💡 Tips for Usage

* Keep all public files inside `www/` to ensure security.
* Avoid placing sensitive data in the served directory.
* Use different ports if running multiple servers simultaneously.
* Test on both localhost and other devices on the same network by using your machine's IP address.
