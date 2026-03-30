#ifndef CLIENT
# define CLIENT

# include <string>
# include <unistd.h>
# include "ConfigParser.hpp"
# include <ctime>

struct ClientState {
    int         fd;
    std::string recv_buffer;    // accumulated received data
    std::string send_buffer;    // data waiting to be sent
    bool        headers_done;   // have the headers been fully read?
    size_t      content_length; // length of the request body
    ServerConfig *config;       // points to the server config for this client

    // CGI
    pid_t       cgi_pid;
    int         cgi_output_fd;
    bool        is_cgi;
    std::string cgi_output;
    time_t      cgi_last_activity;

    ClientState();
};

#endif