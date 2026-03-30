#include "../includes/Webserv.hpp"

void startCGI(const HttpRequest &req, const LocationConfig &loc, ClientState &client) {
    (void)loc;
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

        std::string scriptpath = "./www" + req.path;

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
        } else {
            exit(1);  // Unsupported file type
        }

        char *args[3];
        args[0] = (char*)interpreter.c_str();
        args[1] = (char*)scriptpath.c_str();
        args[2] = NULL;

        std::string method = "REQUEST_METHOD=" + req.method;
        std::string path   = "PATH_INFO=" + req.path;
        std::string query  = "QUERY_STRING=";
        char *env[4];
        env[0] = (char*)method.c_str();
        env[1] = (char*)path.c_str();
        env[2] = (char*)query.c_str();
        env[3] = NULL;

        execve(interpreter_path.c_str(), args, env);
        exit(1);
    }
    else {
        // Parent process: do not wait, just store state
        close(input_pipe[0]);

        // Write POST body to child stdin
        if (!req.body.empty())
            write(input_pipe[1], req.body.c_str(), req.body.size());
        close(input_pipe[1]);
        close(output_pipe[1]);

        // Store CGI state in ClientState
        client.cgi_pid       = pid;
        client.cgi_output_fd = output_pipe[0];
        client.is_cgi        = true;
    }
}

