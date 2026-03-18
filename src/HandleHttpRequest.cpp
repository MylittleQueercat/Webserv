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

std::string handleGET(const HttpRequest &req) 
{
	//1. concanate root and path to get the full file path
	std::string filepath = "./www" + req.path;

	//2. if the requested path is a directory, append index.html
	if (filepath[filepath.size() - 1] == '/')
		filepath += "index.html";

	//3. Security check
		//3.1 check if the path exists and valid
	char resolved[PATH_MAX];
	if (realpath(filepath.c_str(), resolved) == NULL)
		return "HTTP/1.1 404 Not Found\r\n"
				"Content-length: 0\r\n"
				"\r\n";
		//3.2 check if the path still inside ./www/ (which is the server's sendbox)
	char root[PATH_MAX];
	realpath("./www/", root);
	if (std::string(resolved).find(root) != 0)
		return "HTTP/1.1 403 Forbidden\r\n"
				"Content-Length: 0\r\n"
				"\r\n";
		//3.3 check if the file exists and is readable
	std::ifstream file(resolved);//c_str() converts a C++ std::string into a C-style string (a const char*);std::ifstream constructor only accepts const char*
	if (!file.is_open())
		return "HTTP/1.1 404 Not Found\r\n"
				"Content-length: 0\r\n"
				"\r\n";
	
	//4. read the file content
		//std::istreambuf_iterator<char>(file) and std::istreambuf_iterator<char>() are temporary objects (not variables), and they are the two arguments passed to the std::string constructor.
		// This is what's really happening:
		// std::istreambuf_iterator<char> begin(file); // begin iterator
		// std::istreambuf_iterator<char> end;         // end iterator
		// std::string body(begin, end);               // read everything between them

	std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	//5. construct the HTTP response
	std::ostringstream response;
	std::string ContentType = getContentType(resolved); 
	response << "HTTP/1.1 200 OK\r\n";
	response << "Content-Type:" << ContentType << "\r\n";
	response << "Content-Length: " << body.size() << "\r\n";
	response << "\r\n";
	response << body;
	return response.str();
}

std::string handlePOST(const HttpRequest &req)
{
//1. check body is not empty
	if (req.body.empty())
		return "HTTP/1.1 400 Bad Request\r\n"
				"Content-Length: 0\r\n"
				"\r\n";
//2. check body size doesn't exceed max allowed size (max 10mb for exemple)
	if (req.body.size() > 10 * 1024 * 1024)
		return "HTTP/1.1 413 Request Entity TOO Large\r\n"
				"Content-Length: 0\r\n"
				"\r\n";
//3. save the file to ./uploads/
	//3.1 Build the upload filepath
	std::string filepath = "./uploads/uploaded_file";
	//3.2 open the uploaded_file for writing ： looks for ./uploads/ directory；creates a new file named "uploaded_file" inside it；opens it ready for writing
	std::ofstream outfile(filepath.c_str());//creat and open a file at that path for writing
	if (!outfile.is_open())//checks if that operation succeeded or failed; it fails if the directory doesn't exist, or No write permission, or invalid path
		return "HTTP/1.1 500 Internal Server Error\r\n"
				"Content-Length: 0\r\n"
				"\r\n";
	//3.3 Write body to file
	outfile.write(req.body.c_str(), req.body.size());
	outfile.close();
//4. return 201 Created
	return "HTTP/1.1 201 Created\r\n"
			"Content-Length: 0\r\n"
			"\r\n";
}

std::string handleDELETE(const HttpRequest &req)
{
// 1. build the real file path
	std::string filepath = "./uploads" + req.path;
// 2. security check
	// 2.1 resolve the real path
	char resolved[PATH_MAX];
	if (realpath(filepath.c_str(), resolved) == NULL)
		return "HTTP/1.1 404 Not Found\r\n"
				"Content-Length: 0 \r\n"
				"\r\n";
	// 2.2 Check the path is still inside ./uploads/
	char root[PATH_MAX];
	realpath("./uploads", root);
	if (std::string(resolved).find(root) != 0)
		return "HTTP/1.1 403 Forbidden\r\n"
				"Content-Length: 0\r\n"
				"\r\n";
// 3. try to delete the file
	if (remove(resolved) == 0)
		return "HTTP/1.1 204 No Content\r\n"
				"Content-Length: 0\r\n"
				"\r\n";//success
	else
		return "HTTP/1.1 404 Not Found\r\n"
				"Content-Length: 0\r\n"
				"\r\n";
}

std::string handleRequest(const HttpRequest &req)
{
	if (req.method == "GET")
		return handleGET(req);
	else if (req.method == "POST")
		return handlePOST(req);
	else if (req.method == "DELETE")
		return handleDELETE(req);
	else
		return "HTTP/1.1 405 Method Not Allowed\r\n\r\n"; 
}