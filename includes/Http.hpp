#ifndef HTTP_HPP
# define HTTP_HPP

# include <string>
# include <map>

struct HttpRequest {
    std::string method;   // "GET"
    std::string path;     // "/index.html"
    std::string version;  // "HTTP/1.1"
    std::map<std::string, std::string> headers;
    std::string body;
    int         client_fd;
};

HttpRequest parseRequest(const std::string &raw);

#endif