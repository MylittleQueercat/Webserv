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
std::string buildErrorResponse(int code, const ServerConfig &config);
//std::string buildErrorResponse(int code, const std::string &error_page_path);
HttpRequest parseRequest(const std::string &raw);
std::string handleRequest(const HttpRequest &req, const ServerConfig &config, const LocationConfig &loc);

#endif