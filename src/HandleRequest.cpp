#include <poll.h>
#include <vector>
#include <iostream>
#include <unistd.h>
#include<fstream>
#include <set>
#include <sstream>
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
		std::string body ="<html><body><h1>404 Not Found</h1></body></html>";
		std::ostringstream response;
		response << "HTTP/1.1 404 Not Found\r\n";
		response << "Content-Type: text/html\r\n";
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

