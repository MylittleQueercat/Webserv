#include "Server.hpp"

Server::Server() : server_fd(-1)
{
	std::cout << "Server constructor called" << std::endl;
}

Server::~Server()
{
	if (server_fd >= 0)
	{
		std::cout << "Closing socket: " << server_fd << std::endl;
		close(server_fd);
		server_fd = -1;
	}
}

bool Server::setup(const std::string& ip, int port)
{
	//1. create socket
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
	{
		std::cerr << "Error : Socket creation failed" << std::endl;
		return false;
	}
	std::cout << "Socket created: " << server_fd << std::endl;

	// 2. set socket options (SO_REUSEADDR: reuse the previous socket address)
	int opt = 1;//Enable the socket option; opt = 1 or other non-zero value means "turn on" the socket option; opt = 0 means "turn off" the socket option
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		std::cerr << "Error: setsockopt(SO_REUSEADDR) failed" << std::endl;
		close(server_fd);
		server_fd = -1;
		return false;
	}

	// 3. Set non-blocking mode
	int flags = fcntl(server_fd, F_GETFL, 0);
	if (flags < 0)
	{
		std::cerr << "Error: fcntl(F_GETFL) failed" << std::endl;
		close(server_fd);
		server_fd = -1;
		return false;
	}
	if (fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		std::cerr << "Error: fcntl(F_SETFL) failed" << std::endl;
		close(server_fd);
		server_fd = -1;
		return false;
	}
	std::cout << "Socket set to non-blocking mode" << std::endl;

	// 4. Configure server's socket address structure
	struct sockaddr_in address;
	std::memset(&address, 0, sizeof(address));//Fill all bytes of the address structure with zeros
	address.sin_family = AF_INET;// in the IPv4, and use the IPv4 protocol
	address.sin_port = htons(port);//listen on port "port"
	// Convert IP address
	if (ip.empty() || ip == "0.0.0.0") 
	{
		address.sin_addr.s_addr = INADDR_ANY;
		std::cout << "Binding to all interfaces (0.0.0.0)" << std::endl;
    } 
	else 
	{
		if (inet_pton(AF_INET, ip.c_str(), &address.sin_addr) <= 0) 
		{
			std::cerr << "Error: Invalid IP address: " << ip << std::endl;
			close(server_fd);
			server_fd = -1;
			return false;
 	}
 	std::cout << "Binding to: " << ip << std::endl;
	}

    // 5. Bind socket to address
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Error: Bind failed on " << ip << ":" << port << std::endl;
        close(server_fd);
        server_fd = -1;
        return false;
    }
    std::cout << "Socket bound to port: " << port << std::endl;
    
    // 6. Start listening
    if (listen(server_fd, 128) < 0) {
        std::cerr << "Error: Listen failed" << std::endl;
        close(server_fd);
        server_fd = -1;
        return false;
    }
    std::cout << "Server listening (backlog: 128)" << std::endl;
    
    std::cout << "✅ Server setup complete on " << ip << ":" << port << std::endl;
    return true;	
}

// Get file descriptor
int Server::get_fd() const 
{
	return server_fd;
}

/*
#include "Server.hpp"
#include <iostream>

int main() {
    std::cout << "=== Starting Server ===" << std::endl;
    
    Server server;
    
    if (!server.setup("0.0.0.0", 8080)) {
        std::cerr << "Failed to setup server" << std::endl;
        return 1;
    }
    
    std::cout << "\n✅ Server is running on port 8080" << std::endl;
    std::cout << "Server FD: " << server.get_fd() << std::endl;
    
    // Server will automatically close when it goes out of scope
    std::cout << "\n=== Shutting down ===" << std::endl;
    return 0;
}*/