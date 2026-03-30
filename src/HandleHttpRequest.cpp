#include <poll.h>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include "../includes/Server.hpp"
#include "../includes/ConfigParser.hpp"
#include "../includes/Client.hpp"
#include "../includes/Http.hpp"
#include <linux/limits.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

std::string getStatusText(int code) {
    if (code == 400) return "Bad Request";
    if (code == 403) return "Forbidden";
    if (code == 404) return "Not Found";
    if (code == 405) return "Method Not Allowed";
    if (code == 413) return "Content Too Large";
    if (code == 500) return "Internal Server Error";
    return "Error";
}

std::string buildErrorResponse(int code, const ServerConfig &config) {
    std::string error_page_path = "";
    if (config.error_pages.count(code))
        error_page_path = config.error_pages.at(code);
    std::string filepath = "./www" + error_page_path;

    std::ostringstream oss;
    oss << code;
    std::string status = "HTTP/1.1 " + oss.str() + " " + getStatusText(code) + "\r\n";

    if (config.error_pages.count(code)) {
        std::ifstream file(filepath.c_str());
        if (file.is_open()) {
            std::string body((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
            std::ostringstream len;
            len << body.size();
            return status +
                "Content-Type: text/html\r\n"
                "Content-Length: " + len.str() + "\r\n"
                "\r\n" + body;
        }
    }
    // ✅ 文件打不开，返回内置默认页面
    std::string default_body = "<html><body><h1>" + oss.str() + " " + getStatusText(code) + "</h1></body></html>";
    std::ostringstream default_len;
    default_len << default_body.size();
    return status +
           "Content-Type: text/html\r\n"
           "Content-Length: " + default_len.str() + "\r\n"
           "\r\n" + default_body;
}

// std::string getStatusText(int code) {
//     if (code == 400) return "Bad Request";
//     if (code == 403) return "Forbidden";
//     if (code == 404) return "Not Found";
//     if (code == 405) return "Method Not Allowed";
//     if (code == 413) return "Content Too Large";
//     if (code == 500) return "Internal Server Error";
//     return "Error";
// }

// std::string buildErrorResponse(int code, const ServerConfig &config) {
//     // 1. 拼出错误页面路径
    
//     std::string error_page_path = "";
//     if (config.error_pages.count(code))
//         error_page_path = config.error_pages.at(code);
    
//     std::string filepath = "./www" + error_page_path;
    
//     // 2. 尝试读文件
//     std::ifstream file(filepath.c_str());
//     if (file.is_open()) {
//         std::string body((std::istreambuf_iterator<char>(file)),
//                           std::istreambuf_iterator<char>());
//         std::ostringstream oss;
//         oss << code;
//         std::ostringstream len;
//         len << body.size();
//         return "HTTP/1.1 " + oss.str() + " Error\r\n"
//                "Content-Type: text/html\r\n"
//                "Content-Length: " + len.str() + "\r\n"
//                "\r\n" + body;
//     }
    
//     // 3. 文件不存在，返回空响应
//     std::ostringstream oss;
//     oss << code;
//     return "HTTP/1.1 " + oss.str() + " Error\r\n"
//            "Content-Length: 0\r\n\r\n";
// }
// std::string buildErrorResponse(int code, const std::string &error_page_path) {
//     // 1. 拼出错误页面路径
//     std::string filepath = "./www" + error_page_path;
    
//     // 2. 尝试读文件
//     std::ifstream file(filepath.c_str());
//     if (file.is_open()) {
//         std::string body((std::istreambuf_iterator<char>(file)),
//                           std::istreambuf_iterator<char>());
//         std::ostringstream oss;
//         oss << code;
//         std::ostringstream len;
//         len << body.size();
//         return "HTTP/1.1 " + oss.str() + " Error\r\n"
//                "Content-Type: text/html\r\n"
//                "Content-Length: " + len.str() + "\r\n"
//                "\r\n" + body;
//     }
    
//     // 3. 文件不存在，返回空响应
//     std::ostringstream oss;
//     oss << code;
//     return "HTTP/1.1 " + oss.str() + " Error\r\n"
//            "Content-Length: 0\r\n\r\n";
// }

std::string getContentType(const std::string &filepath)
{
	size_t dot = filepath.rfind('.');
	if (dot == std::string::npos)
		return "application/octet-stream";// unknown file type
	std::string ext = filepath.substr(dot);
	if (ext == ".html") return "text/html";
    if (ext == ".css")  return "text/css";
    if (ext == ".js")   return "application/javascript";
    if (ext == ".png")  return "image/png";
    if (ext == ".jpg")  return "image/jpeg";
    if (ext == ".gif")  return "image/gif";
    if (ext == ".txt")  return "text/plain";
    if (ext == ".pdf")  return "application/pdf";
	return "application/octet-stream";
}

#include <dirent.h>
#include <sys/stat.h>

std::string buildAutoindex(const std::string &url_path,
                            const std::string &dir_path,
                            const ServerConfig &config)
{
    DIR *dir = opendir(dir_path.c_str());
    if (!dir)
        return buildErrorResponse(404, config);

    std::string body = "<html><head><title>Index of " + url_path + "</title></head>\n"
                       "<body><h1>Index of " + url_path + "</h1><hr><pre>\n";

    if (url_path != "/")
        body += "<a href=\"../\">../</a>\n";

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == "." || name == "..")
            continue;

        std::string full_path = dir_path + name;
        struct stat st;
        if (stat(full_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            name += "/";

        body += "<a href=\"" + name + "\">" + name + "</a>\n";
    }
    closedir(dir);
    body += "</pre><hr></body></html>";

    std::ostringstream oss;
    oss << body.size();
    return "HTTP/1.1 200 OK\r\n"
           "Content-Type: text/html\r\n"
           "Content-Length: " + oss.str() + "\r\n"
           "\r\n" + body;
}

std::string handleGET(const HttpRequest &req, const ServerConfig &config, const LocationConfig &loc)
{
    std::string filepath = config.root + req.path;
    if (filepath[filepath.size() - 1] == '/') {
        if (loc.autoindex) {
            return buildAutoindex(req.path, filepath, config);
        }
        std::string index = loc.index.empty() ? "index.html" : loc.index;
        filepath += index;
    }

    char resolved[PATH_MAX];
    if (realpath(filepath.c_str(), resolved) == NULL)
        return buildErrorResponse(404, config);  // ← 改

    char root[PATH_MAX];
    realpath(config.root.c_str(), root);
    std::string root_str(root);
    if (root_str[root_str.size() - 1] != '/')
        root_str += '/';
    std::string resolved_str(resolved);
    if (resolved_str != root_str.substr(0, root_str.size() - 1) &&
        resolved_str.find(root_str) != 0)
        return buildErrorResponse(403, config);

    std::ifstream file(resolved);
    if (!file.is_open())
        return buildErrorResponse(404, config);  // ← 改

    std::string body((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
    std::ostringstream response;
    std::string ContentType = getContentType(resolved); 
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type:" << ContentType << "\r\n";
    response << "Content-Length: " << body.size() << "\r\n";
    response << "\r\n";
    response << body;
    return response.str();
}

std::string handlePOST(const HttpRequest &req, const ServerConfig &config, const LocationConfig &loc)
{
    (void)config;


    std::cout << "DEBUG upload_store: [" << loc.upload_store << "]" << std::endl;
    if (req.body.empty())
        return buildErrorResponse(400, config);  // ← 改

    if (req.body.size() > 1 * 1024 * 1024)
        return buildErrorResponse(413, config);  // ← 改

    std::string filepath = loc.upload_store + "/uploaded_file";
    std::ofstream outfile(filepath.c_str());
    if (!outfile.is_open())
        return buildErrorResponse(500, config);  // ← 改

    outfile.write(req.body.c_str(), req.body.size());
    outfile.close();
    return "HTTP/1.1 201 Created\r\n"
           "Content-Length: 0\r\n\r\n";
}

std::string handleDELETE(const HttpRequest &req, const ServerConfig &config, const LocationConfig &loc)
{
    (void)config;

    std::string filename = req.path.substr(req.path.rfind('/'));
    std::string filepath = loc.upload_store + filename;
    char resolved[PATH_MAX];
    if (realpath(filepath.c_str(), resolved) == NULL)
        return buildErrorResponse(404, config);  // ← 改

    char root[PATH_MAX];
    realpath(loc.upload_store.c_str(), root);
    if (std::string(resolved).find(root) != 0)
        return buildErrorResponse(403, config);  // ← 改

    if (remove(resolved) == 0)
        return "HTTP/1.1 204 No Content\r\n"
               "Content-Length: 0\r\n\r\n";
    else
        return buildErrorResponse(404, config);  // ← 改
}

std::string handleRequest(const HttpRequest &req, const ServerConfig &config, const LocationConfig &loc)
{
    if (loc.redirect_code != 0 && !loc.redirect_url.empty()) {
        std::ostringstream oss;
        oss << loc.redirect_code;
        std::string status_text = (loc.redirect_code == 301) ? "Moved Permanently" : "Found";
        return "HTTP/1.1 " + oss.str() + " " + status_text + "\r\n"
               "Location: " + loc.redirect_url + "\r\n"
               "Content-Length: 0\r\n"
               "\r\n";
    }
    if (req.method == "GET")
        return handleGET(req, config, loc);
    else if (req.method == "POST")
        return handlePOST(req, config, loc);
    else if (req.method == "DELETE")
        return handleDELETE(req, config, loc);
    else
        return buildErrorResponse(405, config);  // ← 改
}

// std::string handleGET(const HttpRequest &req) 
// {
// 	//1. concanate root and path to get the full file path
// 	std::string filepath = "./www" + req.path;

// 	//2. if the requested path is a directory, append index.html
// 	if (filepath[filepath.size() - 1] == '/')
// 		filepath += "index.html";

// 	//3. Security check
// 		//3.1 check if the path exists and valid
// 	char resolved[PATH_MAX];
// 	if (realpath(filepath.c_str(), resolved) == NULL)
// 		return "HTTP/1.1 404 Not Found\r\n"
// 				"Content-length: 0\r\n"
// 				"\r\n";
// 		//3.2 check if the path still inside ./www/ (which is the server's sendbox)
// 	char root[PATH_MAX];
// 	realpath("./www/", root);
// 	if (std::string(resolved).find(root) != 0)
// 		return "HTTP/1.1 403 Forbidden\r\n"
// 				"Content-Length: 0\r\n"
// 				"\r\n";
// 		//3.3 check if the file exists and is readable
// 	std::ifstream file(resolved);//c_str() converts a C++ std::string into a C-style string (a const char*);std::ifstream constructor only accepts const char*
// 	if (!file.is_open())
// 		return "HTTP/1.1 404 Not Found\r\n"
// 				"Content-length: 0\r\n"
// 				"\r\n";
	
// 	//4. read the file content
// 		//std::istreambuf_iterator<char>(file) and std::istreambuf_iterator<char>() are temporary objects (not variables), and they are the two arguments passed to the std::string constructor.
// 		// This is what's really happening:
// 		// std::istreambuf_iterator<char> begin(file); // begin iterator
// 		// std::istreambuf_iterator<char> end;         // end iterator
// 		// std::string body(begin, end);               // read everything between them

// 	std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
// 	//5. construct the HTTP response
// 	std::ostringstream response;
// 	std::string ContentType = getContentType(resolved); 
// 	response << "HTTP/1.1 200 OK\r\n";
// 	response << "Content-Type:" << ContentType << "\r\n";
// 	response << "Content-Length: " << body.size() << "\r\n";
// 	response << "\r\n";
// 	response << body;
// 	return response.str();
// }

// std::string handlePOST(const HttpRequest &req)
// {
// //1. check body is not empty
// 	if (req.body.empty())
// 		return "HTTP/1.1 400 Bad Request\r\n"
// 				"Content-Length: 0\r\n"
// 				"\r\n";
// //2. check body size doesn't exceed max allowed size (max 10mb for exemple)
// 	if (req.body.size() > 1 * 1024 * 1024)
// 		return "HTTP/1.1 413 Request Entity TOO Large\r\n"
// 				"Content-Length: 0\r\n"
// 				"\r\n";
// //3. save the file to ./uploads/
// 	//3.1 Build the upload filepath
// 	std::string filepath = "./uploads/uploaded_file";
// 	//3.2 open the uploaded_file for writing ： looks for ./uploads/ directory；creates a new file named "uploaded_file" inside it；opens it ready for writing
// 	std::ofstream outfile(filepath.c_str());//creat and open a file at that path for writing
// 	if (!outfile.is_open())//checks if that operation succeeded or failed; it fails if the directory doesn't exist, or No write permission, or invalid path
// 		return "HTTP/1.1 500 Internal Server Error\r\n"
// 				"Content-Length: 0\r\n"
// 				"\r\n";
// 	//3.3 Write body to file
// 	outfile.write(req.body.c_str(), req.body.size());
// 	outfile.close();
// //4. return 201 Created
// 	return "HTTP/1.1 201 Created\r\n"
// 			"Content-Length: 0\r\n"
// 			"\r\n";
// }

// std::string handleDELETE(const HttpRequest &req)
// {
// // 1. build the real file path
// 	std::string filepath = "./uploads" + req.path;
// // 2. security check
// 	// 2.1 resolve the real path
// 	char resolved[PATH_MAX];
// 	if (realpath(filepath.c_str(), resolved) == NULL)
// 		return "HTTP/1.1 404 Not Found\r\n"
// 				"Content-Length: 0 \r\n"
// 				"\r\n";
// 	// 2.2 Check the path is still inside ./uploads/
// 	char root[PATH_MAX];
// 	realpath("./uploads", root);
// 	if (std::string(resolved).find(root) != 0)
// 		return "HTTP/1.1 403 Forbidden\r\n"
// 				"Content-Length: 0\r\n"
// 				"\r\n";
// // 3. try to delete the file
// 	if (remove(resolved) == 0)
// 		return "HTTP/1.1 204 No Content\r\n"
// 				"Content-Length: 0\r\n"
// 				"\r\n";//success
// 	else
// 		return "HTTP/1.1 404 Not Found\r\n"
// 				"Content-Length: 0\r\n"
// 				"\r\n";
// }

// std::string handleRequest(const HttpRequest &req)
// {
// 	if (req.method == "GET")
// 		return handleGET(req);
// 	else if (req.method == "POST")
// 		return handlePOST(req);
// 	else if (req.method == "DELETE")
// 		return handleDELETE(req);
// 	else
// 		return "HTTP/1.1 405 Method Not Allowed\r\n\r\n"; 
// }

