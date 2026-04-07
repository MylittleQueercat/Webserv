/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGI.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hguo <hguo@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/30 16:51:04 by hguo              #+#    #+#             */
/*   Updated: 2026/04/07 17:10:38 by hguo             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../includes/Webserv.hpp"

// Forks a child process to execute a CGI script
// Uses two pipes: input_pipe for sending POST body to the script,
// output_pipe for reading the script's HTTP response
void startCGI(const HttpRequest &req, const LocationConfig &loc, ClientState &client) {
    int input_pipe[2];
    int output_pipe[2];
    pipe(input_pipe);
    pipe(output_pipe);

    pid_t pid = fork();

    if (pid == 0) {
        // Child process
        close(input_pipe[1]);
        close(output_pipe[0]);
        dup2(input_pipe[0], STDIN_FILENO);
        dup2(output_pipe[1], STDOUT_FILENO);
        close(input_pipe[0]);
        close(output_pipe[1]);

        // Build script path using loc.root if set, otherwise default to ./www
        std::string base = loc.root.empty() ? "./www" : loc.root;
        std::string scriptpath = base + req.path;

        // Select interpreter based on file extension
        std::string interpreter;
        std::string interpreter_path;
        if (req.path.find(".py") != std::string::npos) {
            interpreter      = "python3";
            interpreter_path = "/usr/bin/python3";
        } else if (req.path.find(".php") != std::string::npos) {
            interpreter      = "php";
            interpreter_path = "/usr/bin/php";
        } else if (req.path.find(".sh") != std::string::npos) {
            interpreter      = "bash";
            interpreter_path = "/bin/bash";
        } else if (req.path.find(".bla") != std::string::npos) {
            interpreter      = "cgi_tester";
            interpreter_path = "./cgi_tester";
        } else {
            exit(1);
        }

        char *args[3];
        args[0] = (char*)interpreter.c_str();
        args[1] = (char*)scriptpath.c_str();
        args[2] = NULL;

        std::string method = "REQUEST_METHOD=" + req.method;
        std::string path   = "PATH_INFO=" + req.path;
        std::string query  = "QUERY_STRING=";
        std::string protocol = "SERVER_PROTOCOL=HTTP/1.1";
        char *env[5];
        env[0] = (char*)method.c_str();
        env[1] = (char*)path.c_str();
        env[2] = (char*)query.c_str();
        env[3] = (char*)protocol.c_str();
        env[4] = NULL;

        execve(interpreter_path.c_str(), args, env);
        exit(1);
    }
    else {
        // Parent process: do not wait, just store state
        close(input_pipe[0]);

        close(output_pipe[1]);

        // Store CGI state in ClientState
        client.cgi_pid       = pid;
        client.cgi_output_fd = output_pipe[0];
        client.cgi_input_fd  = input_pipe[1];
        client.is_cgi        = true;
        // std::cerr << "startCGI done: cgi_input_fd=" << client.cgi_input_fd << std::endl;
    }
}

