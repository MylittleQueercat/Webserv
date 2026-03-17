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

std::string handleRequest(const HttpRequest &req) 
{
	if (req.method == "GET")
		return handleGET();
	else if (req.method == "POST")
		return handlePOST();
	else if (req.method == "DELETE")
		return handleDELETE();
	else
		return "HTTP/1.1 405 Method Not Allowed\r\n\r\n"; 
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

std::string handleGET(const HttpRequest &req) 
{
	//1. concanate root and path to get the full file path
	std::string filepath = "./www" + req.path;

	//2. if the requested path is a directory, append index.html
	if (filepath[filepath.size() - 1] == '/')
		filepath += "index.html";

	//3. check if the file exists and is readable
	std::ifstream file(filepath.c_str());//c_str() converts a C++ std::string into a C-style string (a const char*);std::ifstream constructor only accepts const char*
	if (!file.is_open()) // the file doesn't exist or can't be opened
	{
		std::string contentType = getContentType(filepath);
		std::string body ="<html><body><h1>404 Not Found</h1></body></html>";
		std::ostringstream response;
		response << "HTTP/1.1 404 Not Found\r\n";
		response << "Content-Type:" << contentType << "\r\n";
		response << "Content-Length: " << body.size() << "\r\n";
		response << "\r\n";
		response << body;
		return response.str();
	}
	//4. read the file content
		//std::istreambuf_iterator<char>(file) and std::istreambuf_iterator<char>() are temporary objects (not variables), and they are the two arguments passed to the std::string constructor.
		//This is what's really happening:
		// This is what's really happening:
		// std::istreambuf_iterator<char> begin(file); // begin iterator
		// std::istreambuf_iterator<char> end;         // end iterator
		// std::string body(begin, end);               // read everything between them

	std::string body((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	//5. construct the HTTP response
	std::ostringstream response;
	response << "HTTP/1.1 200 OK\r\n";
	response << "Content-Type: text/html\r\n";//picture?
	response << "Content-Length: " << body.size() << "\r\n";
	response << "\r\n";
	response << body;
	return response.str();
	//
}

