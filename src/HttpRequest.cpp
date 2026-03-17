#include "../includes/Http.hpp"
#include <sstream>

HttpRequest parseRequest(const std::string &raw) {
    HttpRequest req;

    // find first line
    size_t first_line_end = raw.find("\r\n");
    std::string first_line = raw.substr(0, first_line_end);
    
    // Read headers line by line
    size_t pos = first_line_end + 2;
    while (pos < raw.size()) {
        size_t line_end = raw.find("\r\n", pos);
        std::string line = raw.substr(pos, line_end - pos);

        if (line.empty())
            break;

        // separate key/value
        size_t colon = line.find(":");
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            size_t v_start = value.find_first_not_of(" \t");
            if (v_start != std::string::npos)
                value = value.substr(v_start);
            req.headers[key] = value;
        }
        pos = line_end + 2;
    }

    // Get method/path/version
    std::istringstream iss(first_line);
    iss >> req.method >> req.path >> req.version;

    //if content-Length exists -> read body
    size_t header_end = raw.find("\r\n\r\n");
    if (header_end != std::string::npos) {
        size_t body_start = header_end + 4;//请求行+请求头+/r/n/r/n

        if (req.headers.count("Content-Length")) {
            size_t content_length = atoi(req.headers["Content-Length"].c_str());

            if (body_start + content_length <= raw.size())
                req.body = raw.substr(body_start, content_length);
        }
    }

    return req;
}