#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

bool send_all(int sock_fd, const std::string& data) {
    size_t total_sent = 0;
    while (total_sent < data.size()) {
        ssize_t sent = send(sock_fd, data.data() + total_sent, data.size() - total_sent, 0);
        if (sent <= 0) return false;
        total_sent += sent;
    }
    return true;
}

std::string read_one_response(int sock_fd, std::string& buffer) {
    char temp[4096];
    while (true) {
        size_t header_end = buffer.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            size_t content_length = 0;
            std::string headers = buffer.substr(0, header_end);
            
            std::string lower_headers = headers;
            for (char& c : lower_headers) c = std::tolower(static_cast<unsigned char>(c));
            size_t cl_pos = lower_headers.find("content-length:");
            if (cl_pos != std::string::npos) {
                size_t line_end = lower_headers.find("\r\n", cl_pos);
                std::string cl_val = headers.substr(cl_pos + 15, line_end - (cl_pos + 15));
                content_length = std::strtoul(cl_val.c_str(), nullptr, 10);
            }

            size_t total_len = header_end + 4 + content_length;
            if (buffer.size() >= total_len) {
                std::string response = buffer.substr(0, total_len);
                buffer.erase(0, total_len);
                return response;
            }
        }

        ssize_t bytes_read = recv(sock_fd, temp, sizeof(temp), 0);
        if (bytes_read <= 0) {
            return "";
        }
        buffer.append(temp, bytes_read);
    }
}

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = std::atoi(argv[1]);
    }

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }

    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    std::cout << "Connecting to 127.0.0.1:" << port << "...\n";
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed. Is the server running?\n";
        close(sock_fd);
        return 1;
    }
    std::cout << "Connected! [1 TCP Connection Established]\n\n";

    struct TestCase {
        std::string name;
        std::string raw_request;
        std::string expected_status;
        std::string expected_body;
    };

    std::vector<TestCase> test_cases = {
        {"GET /add?a=2&b=3", "GET /add?a=2&b=3 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", "200 OK", "5"},
        {"GET /sub?a=10&b=4", "GET /sub?a=10&b=4 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", "200 OK", "6"},
        {"GET /mul?a=6&b=7", "GET /mul?a=6&b=7 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", "200 OK", "42"},
        {"GET /div?a=9&b=3", "GET /div?a=9&b=3 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", "200 OK", "3"},
        {"GET /pow?a=2&b=8", "GET /pow?a=2&b=8 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", "200 OK", "256"},
        {"GET /div?a=1&b=0 (div by zero)", "GET /div?a=1&b=0 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", "400 Bad Request", "Division by zero"},
        {"GET /add?a=x&b=3 (invalid param)", "GET /add?a=x&b=3 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", "400 Bad Request", "Invalid integer parameter"},
        {"GET /mod?a=5&b=2 (unknown endpoint)", "GET /mod?a=5&b=2 HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", "404 Not Found", "Not Found"},
        {"POST /add (unsupported method)", "POST /add HTTP/1.1\r\nHost: localhost:8080\r\n\r\n", "405 Method Not Allowed", "Method Not Allowed"},
        {"GET /add (missing host header)", "GET /add?a=2&b=3 HTTP/1.1\r\n\r\n", "400 Bad Request", "Missing Host Header"}
    };

    std::string client_buffer;
    int passed = 0;

    for (size_t i = 0; i < test_cases.size(); ++i) {
        std::cout << ">>> Request " << (i + 1) << ": " << test_cases[i].name << "\n";
        if (!send_all(sock_fd, test_cases[i].raw_request)) {
            std::cerr << "FAIL: Could not send request on socket.\n";
            break;
        }

        std::string response = read_one_response(sock_fd, client_buffer);
        if (response.empty()) {
            std::cerr << "FAIL: Connection closed prematurely by server.\n";
            break;
        }

        std::cout << "<<< Response:\n" << response << "\n";

        bool ok_status = (response.find(test_cases[i].expected_status) != std::string::npos);
        bool ok_body = (response.find(test_cases[i].expected_body) != std::string::npos);

        if (ok_status && ok_body) {
            std::cout << "[PASS]\n\n";
            passed++;
        } else {
            std::cout << "[FAIL] (Expected: " << test_cases[i].expected_status << ")\n\n";
        }
    }

    std::cout << "========================================\n";
    std::cout << passed << " / " << test_cases.size() << " requests passed on the same TCP socket.\n";
    std::cout << "All requests were handled over one TCP connection.\n";
    std::cout << "========================================\n";

    close(sock_fd);
    return (passed == static_cast<int>(test_cases.size())) ? 0 : 1;
}
