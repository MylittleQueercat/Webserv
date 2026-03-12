#ifndef SERVER_H
# define SERVER_H

# include <sys/socket.h> // socket(), bind(), listen(), setsockopt()
# include <netinet/in.h> // struct sockaddr_in, htons(), INADDR_ANY
# include <arpa/inet.h> // inet_pton(), inet_ntop()
# include <unistd.h> // close(), read(), write()
# include <fcntl.h> // fcntl(), O_NONBLOCK
# include <iostream>
# include <cstring>
class Server {
private:
	int server_fd; // stores socket file descriptor
	// Prevent copying
	Server(const Server& other);
	Server& operator=(const Server& other);
public:
	Server();
	~Server();
	// Setup server (create, bind, listen)
	bool setup(const std::string& ip, int port);
	// Get socket file descriptor
	int get_fd() const;
};

#endif