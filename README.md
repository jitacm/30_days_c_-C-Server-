# 🌐 Simple HTTP Server in C (WSL Debian)

This project implements a lightweight HTTP server written in the C programming language. It is designed to run inside the **Debian distribution on Windows Subsystem for Linux (WSL)**. The server listens for incoming HTTP GET requests and serves a static HTML file in response.

This setup is ideal for learning low-level network programming, exploring how web servers work, and building minimalistic development environments using native Linux tools on a Windows system.

---

## 📚 Overview

This HTTP server demonstrates:

- Basic usage of **sockets** in C.

- Parsing and handling of **HTTP GET requests**.

- Serving static HTML content to a browser.

- Running C applications inside **Debian on WSL**, providing a Linux-like experience on Windows.

---

## 🧱 Project Structure

```
project-root/
├── server.c         # Main source code implementing the HTTP server
├── index.html       # HTML file served when a GET request is received
└── README.md        # Documentation and usage instructions
```

---

## ⚙️ Requirements

To build and run this project, ensure you have the following installed:

- **Windows 10/11** with **WSL** enabled

- **Debian distribution** installed via Microsoft Store or manually

- Required development packages:

```bash
sudo apt update
sudo apt install build-essential
```

(Optional but recommended):

- **Visual Studio Code** with the **Remote - WSL** extension for editing and terminal integration

---

## 🛠️ Setup and Compilation

1. **Open the Debian terminal** (WSL).

2. **Navigate to the project directory**:

   ```bash
   cd /path/to/your/project
   ```

3. **Compile the server using GCC**:

   ```bash
   gcc server.c -o server -lpthread
   ```

4. **Run the server**:

   ```bash
   ./server
   ```

   You should see a message indicating that the server is running on port 8080.

---

## 🌐 Accessing the Server

Once the server is running, open your web browser and go to:

```
http://localhost:8080
```

You will see the contents of the `index.html` file displayed in the browser.

---

## 📖 How It Works

- The server listens on **port 8080** using a TCP socket.

- When a connection is received, it checks if the request is a **GET** method.

- If valid, the contents of `index.html` are read and sent as the HTTP response.

- The connection is then closed, and the server continues to wait for new requests.

---

## 💡 Educational Value

This project serves as a great introduction to:

- Networking and socket programming in C

- HTTP protocol basics

- Linux development environment inside Windows using WSL

- Serving static files with custom server logic

---

## 📸 Example Output

Once running, the server terminal might show logs like:

```
Server is listening on port 8080...
Received request: GET / HTTP/1.1
Sent index.html to client.
```

And the browser will display the content from `index.html`.

---

