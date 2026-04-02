/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HandleHttpRequest.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hguo <hguo@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/30 16:55:36 by jili              #+#    #+#             */
/*   Updated: 2026/04/02 13:52:26 by hguo             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../includes/Webserv.hpp"

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
    // File not found or no error_page configured, return built-in default error page
    std::string default_body = "<html><body><h1>" + oss.str() + " " + getStatusText(code) + "</h1></body></html>";
    std::ostringstream default_len;
    default_len << default_body.size();
    return status +
           "Content-Type: text/html\r\n"
           "Content-Length: " + default_len.str() + "\r\n"
           "\r\n" + default_body;
}

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
    //std::string filepath = config.root + req.path;
    std::string base = loc.root.empty() ? config.root : loc.root;

    // Strip location prefix from req.path
    std::string relative_path = req.path;

    std::string loc_prefix = loc.path;

    // Remove trailing slash from loc_prefix for comparison
    if (!loc_prefix.empty() && loc_prefix[loc_prefix.size() - 1] == '/')
        loc_prefix = loc_prefix.substr(0, loc_prefix.size() - 1);

    if (!loc_prefix.empty() && relative_path.find(loc_prefix) == 0)
        relative_path = relative_path.substr(loc_prefix.size()); // e.g. "" or "/something"

    if (relative_path.empty())
        relative_path = "/";

    // if (!loc.path.empty() && relative_path.find(loc.path) == 0)
    //     relative_path = relative_path.substr(loc.path.size() - 1); // keep leading /

    std::string filepath = base + relative_path;
    
    // std::string filepath = base + req.path;
    
    //commentaires
    std::cerr << "DEBUG filepath: [" << filepath << "]" << std::endl;
    std::cerr << "DEBUG loc.root: [" << loc.root << "]" << std::endl;
    std::cerr << "DEBUG config.root: [" << config.root << "]" << std::endl;
    
    if (filepath[filepath.size() - 1] != '/') 
    {
        struct stat st;
        if (stat(filepath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            filepath += '/';
    }

    if (filepath[filepath.size() - 1] == '/') 
    {
        if (loc.autoindex)
            return buildAutoindex(req.path, filepath, config);
        std::string index = loc.index.empty() ? "index.html" : loc.index;
        filepath += index;
    }

    char resolved[PATH_MAX];
    if (realpath(filepath.c_str(), resolved) == NULL)
        return buildErrorResponse(404, config);

    char root[PATH_MAX];
    std::string root_to_check = loc.root.empty() ? config.root : loc.root;
    realpath(root_to_check.c_str(), root);
    // realpath(config.root.c_str(), root);
    std::string root_str(root);
    if (root_str[root_str.size() - 1] != '/')
        root_str += '/';
    std::string resolved_str(resolved);
    if (resolved_str != root_str.substr(0, root_str.size() - 1) &&
        resolved_str.find(root_str) != 0)
        return buildErrorResponse(403, config);

    std::ifstream file(resolved);
    if (!file.is_open())
        return buildErrorResponse(404, config);

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
        return buildErrorResponse(400, config);

    if (req.body.size() > 1 * 1024 * 1024)
        return buildErrorResponse(413, config);

    std::string filepath = loc.upload_store + "/uploaded_file";
    std::ofstream outfile(filepath.c_str());
    if (!outfile.is_open())
        return buildErrorResponse(500, config);

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
        return buildErrorResponse(404, config);

    char root[PATH_MAX];
    realpath(loc.upload_store.c_str(), root);
    if (std::string(resolved).find(root) != 0)
        return buildErrorResponse(403, config);

    if (remove(resolved) == 0)
        return "HTTP/1.1 204 No Content\r\n"
               "Content-Length: 0\r\n\r\n";
    else
        return buildErrorResponse(404, config);
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
        return buildErrorResponse(405, config);
}

