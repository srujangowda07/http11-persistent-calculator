# HTTP/1.1 Persistent Calculator Server

A simple C++ calculator HTTP server built using raw POSIX TCP sockets. It handles multiple HTTP/1.1 requests over a single persistent TCP connection.

## What this Does

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

## Build

Compile the project using `make`:

```bash
make
```

To remove the compiled binaries:

```bash
make clean
```

## Run

Start the server on port 8080 (default):

```bash
./server
```

Or specify a port:

```bash
./server 9000
```

## How to Run test_client

In a separate terminal, run the test client:

```bash
./test_client
```

Or with a custom port matching the server:

```bash
./test_client 9000
```

The test client opens one TCP connection, executes 10 sequential test requests over the same socket, and checks the status code and body of each response.

## curl Examples

```bash
# Arithmetic operations
curl -v "http://localhost:8080/add?a=2&b=3"
curl -v "http://localhost:8080/sub?a=10&b=4"
curl -v "http://localhost:8080/mul?a=6&b=7"
curl -v "http://localhost:8080/div?a=9&b=3"
curl -v "http://localhost:8080/pow?a=2&b=8"

# Error handling
curl -v "http://localhost:8080/div?a=1&b=0"
curl -v "http://localhost:8080/add?a=x&b=3"
curl -v "http://localhost:8080/unknown"
curl -v -X POST "http://localhost:8080/add?a=2&b=3"
```

## Why the Connection Stays Open

In HTTP/1.1, connections are persistent (`keep-alive`) by default. After sending a response, the server does not close the client socket descriptor. It keeps waiting for new data on the same socket until the client closes the connection or sends a `Connection: close` header. This saves the overhead of repeating TCP handshakes for each request.

## Why recv() Is Not Equal to One HTTP Request

TCP is a byte-stream protocol and has no concept of message boundaries. A single call to `recv()` might read only part of a request, an entire request, or multiple requests bundled together. To handle this:
1. Incoming bytes are appended to a receive buffer.
2. The server scans for `\r\n\r\n` to know when headers are complete.
3. If a request body is expected based on `Content-Length`, the server waits until the full body is received.
4. The server extracts and processes one complete request, then removes only those consumed bytes from the buffer, leaving any remaining bytes for subsequent requests.

## Project Structure

```
http11-persistent-calculator/
├── src/
│   └── main.cpp         # Calculator server implementation
├── tests/
│   └── test_client.cpp  # Test client verifying persistent connection
├── Makefile             # Build instructions
├── .gitignore           # Ignored build files
└── README.md            # Project documentation
```
