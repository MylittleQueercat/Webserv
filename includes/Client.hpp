/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jili <jili@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/30 17:03:34 by jili              #+#    #+#             */
/*   Updated: 2026/03/30 17:03:35 by jili             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT
# define CLIENT

# include <string>
# include <unistd.h>
# include "ConfigParser.hpp"
# include <ctime>

//ClientState is a snapshot of everything that the server needs to know about on connected client
struct ClientState {
    int         fd;
    std::string recv_buffer;    // accumulated received data
    std::string send_buffer;    // data waiting to be sent
    bool        headers_done;   // have the headers been fully read?
    size_t      content_length; // length of the request body
    ServerConfig *config;       // points to the server config for this client

    // CGI
    pid_t       cgi_pid;
    int         cgi_output_fd;// Source: the pipe the server reads raw bytes from CGI process
    bool        is_cgi;
    std::string cgi_output;//Destination: the string stores the raw bytes
    time_t      cgi_last_activity;

    ClientState();
};

#endif