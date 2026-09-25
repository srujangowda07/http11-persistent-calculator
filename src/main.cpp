#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string to_lower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return str;
}

bool parse_int(const std::string& str, long long& out_val) {
    if (str.empty()) return false;
    size_t i = 0;
    if (str[0] == '+' || str[0] == '-') {
        i = 1;
        if (str.length() == 1) return false;
    }
    for (; i < str.length(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(str[i]))) {
            return false;
        }
    }
    try {
        out_val = std::stoll(str);
        return true;
    } catch (...) {
        return false;
    }
}

std::map<std::string, std::string> parse_query(const std::string& query_str) {
    std::map<std::string, std::string> params;
    std::istringstream stream(query_str);
    std::string pair;
    while (std::getline(stream, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = pair.substr(0, eq_pos);
            std::string val = pair.substr(eq_pos + 1);
            params[key] = val;
        } else if (!pair.empty()) {
            params[pair] = "";
        }
    }
    return params;
}

bool send_all(int sock_fd, const std::string& data) {
    size_t total_sent = 0;
    while (total_sent < data.size()) {
        ssize_t sent = send(sock_fd, data.data() + total_sent, data.size() - total_sent, 0);
        if (sent <= 0) {
            return false;
        }
        total_sent += sent;
    }
    return true;
}

std::string build_response(int status_code, const std::string& status_text, const std::string& body, bool keep_alive) {
    std::ostringstream res;
    res << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
    res << "Content-Length: " << body.size() << "\r\n";
    res << "Connection: " << (keep_alive ? "keep-alive" : "close") << "\r\n";
    res << "\r\n";
    res << body;
    return res.str();
}

std::string handle_http_request(const std::string& header_str, [[maybe_unused]] const std::string& body, bool& out_keep_alive) {
    out_keep_alive = true;

    std::istringstream stream(header_str);
    std::string request_line;
    if (!std::getline(stream, request_line)) {
        return build_response(400, "Bad Request", "Bad Request", out_keep_alive);
    }
    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }

    std::istringstream line_stream(request_line);
    std::string method, target, version;
    if (!(line_stream >> method >> target >> version)) {
        return build_response(400, "Bad Request", "Bad Request", out_keep_alive);
    }

    if (version != "HTTP/1.1") {
        return build_response(400, "Bad Request", "Unsupported HTTP version", out_keep_alive);
    }

    std::map<std::string, std::string> headers;
    std::string header_line;
    while (std::getline(stream, header_line)) {
        if (!header_line.empty() && header_line.back() == '\r') {
            header_line.pop_back();
        }
        if (header_line.empty()) continue;

        size_t colon_pos = header_line.find(':');
        if (colon_pos == std::string::npos) {
            return build_response(400, "Bad Request", "Malformed header", out_keep_alive);
        }
        std::string key = to_lower(trim(header_line.substr(0, colon_pos)));
        if (key.empty()) {
            return build_response(400, "Bad Request", "Malformed header", out_keep_alive);
        }
        std::string value = trim(header_line.substr(colon_pos + 1));
        headers[key] = value;
    }

    if (headers.count("connection") && to_lower(headers["connection"]) == "close") {
        out_keep_alive = false;
    }

    if (headers.find("host") == headers.end() || headers["host"].empty()) {
        return build_response(400, "Bad Request", "Missing Host Header", out_keep_alive);
    }

    if (method != "GET") {
        return build_response(405, "Method Not Allowed", "Method Not Allowed", out_keep_alive);
    }

    std::string path = target;
    std::string query_str = "";
    size_t qmark_pos = target.find('?');
    if (qmark_pos != std::string::npos) {
        path = target.substr(0, qmark_pos);
        query_str = target.substr(qmark_pos + 1);
    }

    if (path != "/add" && path != "/sub" && path != "/mul" && path != "/div" && path != "/pow") {
        return build_response(404, "Not Found", "Not Found", out_keep_alive);
    }

    std::map<std::string, std::string> params = parse_query(query_str);
    if (params.find("a") == params.end() || params.find("b") == params.end()) {
        return build_response(400, "Bad Request", "Missing parameters", out_keep_alive);
    }

    long long a = 0, b = 0;
    if (!parse_int(params["a"], a) || !parse_int(params["b"], b)) {
        return build_response(400, "Bad Request", "Invalid integer parameter", out_keep_alive);
    }

    long long result = 0;
    if (path == "/add") {
        result = a + b;
    } else if (path == "/sub") {
        result = a - b;
    } else if (path == "/mul") {
        result = a * b;
    } else if (path == "/div") {
        if (b == 0) {
            return build_response(400, "Bad Request", "Division by zero", out_keep_alive);
        }
        result = a / b;
    } else if (path == "/pow") {
        if (b < 0) {
            return build_response(400, "Bad Request", "Negative exponent", out_keep_alive);
        }
        result = 1;
        long long base = a;
        long long exp = b;
        while (exp > 0) {
            if (exp % 2 == 1) {
                result *= base;
            }
            base *= base;
            exp /= 2;
        }
    }

    return build_response(200, "OK", std::to_string(result), out_keep_alive);
}

void handle_client(int client_fd) {
    // TCP is a byte stream: recv() may return partial data, one request, or multiple requests
    std::string buffer;
    char temp[4096];

    // The client socket stays open across requests to support HTTP/1.1 persistent connections
    while (true) {
        ssize_t bytes_read = recv(client_fd, temp, sizeof(temp), 0);
        if (bytes_read <= 0) {
            break;
        }

        buffer.append(temp, bytes_read);

        // Extract complete requests from the receive buffer while keeping any remaining bytes
        while (true) {
            size_t header_end = buffer.find("\r\n\r\n");
            if (header_end == std::string::npos) {
                break;
            }

            std::string header_str = buffer.substr(0, header_end);
            size_t content_length = 0;

            std::string lower_headers = to_lower(header_str);
            size_t cl_pos = lower_headers.find("content-length:");
            if (cl_pos != std::string::npos) {
                size_t line_end = lower_headers.find("\r\n", cl_pos);
                std::string cl_val = trim(header_str.substr(cl_pos + 15, line_end - (cl_pos + 15)));
                long long cl_num = 0;
                if (!parse_int(cl_val, cl_num) || cl_num < 0) {
                    bool keep_alive = false;
                    std::string response = build_response(400, "Bad Request", "Invalid Content-Length", keep_alive);
                    send_all(client_fd, response);
                    return;
                }
                content_length = static_cast<size_t>(cl_num);
            }

            size_t total_req_len = header_end + 4 + content_length;
            if (buffer.size() < total_req_len) {
                break;
            }

            std::string body = buffer.substr(header_end + 4, content_length);
            buffer.erase(0, total_req_len);

            bool keep_alive = true;
            std::string response = handle_http_request(header_str, body, keep_alive);

            if (!send_all(client_fd, response)) {
                return;
            }

            if (!keep_alive) {
                return;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    int port = 8080;
    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "Invalid port number: " << argv[1] << "\n";
            return 1;
        }
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Bind failed on port " << port << "\n";
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        std::cerr << "Listen failed\n";
        close(server_fd);
        return 1;
    }

    std::cout << "Server listening on port " << port << "...\n";

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            continue;
        }

        // Maintain persistent connection until client disconnects or requests close
        handle_client(client_fd);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
