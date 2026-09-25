# HTTP/1.1 Persistent Calculator Server

A simple C++ calculator HTTP server built using raw POSIX TCP sockets. It handles multiple HTTP/1.1 requests over a single persistent TCP connection.

## What the Project Does

This project implements a basic HTTP/1.1 calculator server that performs arithmetic calculations using raw TCP sockets. The server keeps the client connection open across multiple requests rather than closing the socket after sending each response.

## Supported Endpoints

- `GET /add?a=<int>&b=<int>` : Adds `a` and `b`
- `GET /sub?a=<int>&b=<int>` : Subtracts `b` from `a`
- `GET /mul?a=<int>&b=<int>` : Multiplies `a` and `b`
- `GET /div?a=<int>&b=<int>` : Integer division `a / b`
- `GET /pow?a=<int>&b=<int>` : Power `a ^ b`

## Status Codes

- `200 OK`: Calculation completed successfully with the result in the response body.
- `400 Bad Request`: Returned when the `Host` header is missing, parameters `a` or `b` are missing or not integers, division by zero occurs, negative exponent in `/pow`, the HTTP version is not `HTTP/1.1`, headers are malformed, or `Content-Length` is invalid.
- `404 Not Found`: Returned when requesting an unknown path.
- `405 Method Not Allowed`: Returned for any HTTP method other than `GET`.

## Environment Requirements

This project uses standard POSIX socket APIs (`sys/socket.h`, `unistd.h`, `netinet/in.h`, `arpa/inet.h`). It is designed to run in **Linux / WSL (Ubuntu) / macOS**.

If you are on Windows, run these commands inside **WSL** (or prefix with `wsl`).

Ensure `build-essential` is installed in WSL/Ubuntu:
```bash
sudo apt update && sudo apt install -y build-essential
```

## Build Instructions

### Option 1: Inside Linux / WSL Terminal
```bash
make
```
Or build directly with `g++`:
```bash
g++ -std=c++17 -Wall -Wextra src/main.cpp -o server
g++ -std=c++17 -Wall -Wextra tests/test_client.cpp -o test_client
```

To clean compiled binaries:
```bash
make clean
```

### Option 2: Directly from Windows PowerShell
```powershell
wsl make
```

---

## Run Instructions

### In Linux / WSL:

1. **Start the server (Terminal 1):**
   ```bash
   ./server
   ```
   *(Or specify a custom port: `./server 9000`)*

2. **Run the test client (Terminal 2):**
   ```bash
   ./test_client
   ```
   *(Or specify matching port: `./test_client 9000`)*

### Directly from Windows PowerShell:

1. **Start server:**
   ```powershell
   wsl ./server
   ```

2. **Run test client (second PowerShell window):**
   ```powershell
   wsl ./test_client
   ```

## curl Examples

```bash
# Valid calculations (200 OK)
curl -v "http://localhost:8080/add?a=2&b=3"
curl -v "http://localhost:8080/sub?a=10&b=4"
curl -v "http://localhost:8080/mul?a=6&b=7"
curl -v "http://localhost:8080/div?a=9&b=3"
curl -v "http://localhost:8080/pow?a=2&b=8"

# Error handling (400 Bad Request)
curl -v "http://localhost:8080/div?a=1&b=0"
curl -v "http://localhost:8080/pow?a=2&b=-1"
curl -v "http://localhost:8080/add?a=x&b=3"
curl -v "http://localhost:8080/add?a=5"

# Unknown endpoint (404 Not Found)
curl -v "http://localhost:8080/mod?a=5&b=2"

# Unsupported method (405 Method Not Allowed)
curl -v -X POST "http://localhost:8080/add?a=2&b=3"
```

## Why the Connection Stays Open

In HTTP/1.1, connections are persistent (`keep-alive`) by default. After sending a response, the server keeps the client socket open and waits for the next request. This avoids the overhead of repeating TCP handshakes (`SYN`, `SYN-ACK`, `ACK`) for every HTTP interaction. The socket is closed only when the client disconnects or sends a `Connection: close` header.

## Why recv() Is Not Equal to One HTTP Request

TCP is a stream-oriented protocol without message boundaries. A single `recv()` call may return only a partial request, exactly one request, or multiple requests concatenated together. To handle this properly:
1. Bytes read from `recv()` are appended to a receive buffer.
2. The server scans for `\r\n\r\n` to determine the end of headers.
3. If `Content-Length` is present, the server waits until the entire body is buffered.
4. Exactly one request is parsed and removed from the buffer, leaving leftover bytes for subsequent requests.

## Project Structure

```
http11-persistent-calculator/
├── src/
│   └── main.cpp         # Server implementation
├── tests/
│   └── test_client.cpp  # Persistent connection test client
├── Makefile             # Build automation
├── .gitignore           # Git ignore rules
└── README.md            # Project documentation
```

## Scope & Limitations

- **Sequential Handling:** Handles one connected client connection at a time.
- **HTTP Subset:** Implements only the subset required by the assignment (`GET` arithmetic calculator operations, status codes `200`, `400`, `404`, `405`, `Host` header validation, and `Content-Length` framing).
