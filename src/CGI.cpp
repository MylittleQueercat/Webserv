/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGI.cpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: lenovo <lenovo@student.42.fr>              +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/30 16:51:04 by hguo              #+#    #+#             */
/*   Updated: 2026/04/28 14:10:57 by lenovo           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../includes/Webserv.hpp"

static std::string toUpperHeaderName(const std::string& key)
{
    std::string result;

    for (size_t i = 0; i < key.size(); i++)
    {
        if (key[i] == '-')
            result += '_';
        else
            result += static_cast<char>(std::toupper(static_cast<unsigned char>(key[i])));
    }

    return result;
}

static std::string toLowerString(const std::string& s)
{
    std::string result = s;

    for (size_t i = 0; i < result.size(); i++)
        result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));

    return result;
}


void cleanupCGI(ClientState& client,
                       std::vector<struct pollfd>& fds,
                       size_t& i)
{
    // 关闭并移除 cgi_output_fd（调用者的 fds[i] 就是它，已经处理）
    if (client.cgi_output_fd != -1) {
        // 注意：如果调用者已经 close 了就不要重复 close
        // 统一在这里做，调用者不要单独 close
        close(client.cgi_output_fd);
        for (size_t j = 0; j < fds.size(); j++) {
            if (fds[j].fd == client.cgi_output_fd) {
                fds.erase(fds.begin() + j);
                if (j <= i) i--;
                break;
            }
        }
        client.cgi_output_fd = -1;
    }

    // 关闭并移除 cgi_input_fd
    if (client.cgi_input_fd != -1) {
        close(client.cgi_input_fd);
        for (size_t j = 0; j < fds.size(); j++) {
            if (fds[j].fd == client.cgi_input_fd) {
                fds.erase(fds.begin() + j);
                if (j <= i) i--;
                break;
            }
        }
        client.cgi_input_fd = -1;
    }

    // 回收子进程，避免僵尸进程
    if (client.cgi_pid > 0) {
        kill(client.cgi_pid, SIGKILL);
        waitpid(client.cgi_pid, NULL, 0); // 阻塞等待，确保回收
        client.cgi_pid = -1;
    }

    client.is_cgi = false;
}
// Forks a child process to execute a CGI script
// Uses two pipes: input_pipe for sending POST body to the script,
// output_pipe for reading the script's HTTP response
void startCGI(const HttpRequest &req, const LocationConfig &loc, ClientState &client, bool keep_stdin_open) {
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
        std::string relative_path = req.path;

        // if (relative_path.find(loc.path) == 0)
        //     relative_path = relative_path.substr(loc.path.length());
        if (!relative_path.empty() && relative_path[0] == '/')
            relative_path = relative_path.substr(1);
        if (!base.empty() && base[base.length() - 1] != '/')
            base += "/";

        std::string scriptpath = base + relative_path;

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

        std::vector<std::string> env_strings;

        env_strings.push_back("REQUEST_METHOD=" + req.method);
        env_strings.push_back("REQUEST_URI=" + req.path);
        env_strings.push_back("PATH_INFO=" + req.path);
        env_strings.push_back("SCRIPT_NAME=" + req.path);
        env_strings.push_back("SCRIPT_FILENAME=" + scriptpath);
        env_strings.push_back("PATH_TRANSLATED=" + scriptpath);
        env_strings.push_back("QUERY_STRING=");
        env_strings.push_back("SERVER_PROTOCOL=HTTP/1.1");
        env_strings.push_back("GATEWAY_INTERFACE=CGI/1.1");
        env_strings.push_back("REDIRECT_STATUS=200");

        for (std::map<std::string, std::string>::const_iterator it = req.headers.begin();
            it != req.headers.end(); ++it)
        {
            std::string lower_key = toLowerString(it->first);

            if (lower_key == "content-type")
            {
                env_strings.push_back("CONTENT_TYPE=" + it->second);
            }
            else if (lower_key == "content-length")
            {
                env_strings.push_back("CONTENT_LENGTH=" + it->second);
            }
            else
            {
                std::string env_name = "HTTP_" + toUpperHeaderName(it->first);
                env_strings.push_back(env_name + "=" + it->second);
            }
        }

        std::vector<char*> env;
        for (size_t i = 0; i < env_strings.size(); i++)
            env.push_back(const_cast<char*>(env_strings[i].c_str()));
        env.push_back(NULL);

        execve(interpreter_path.c_str(), args, &env[0]);

        // std::cerr << "[CGI execve failed] path=[" << interpreter_path
        //         << "] errno=" << errno
        //         << " error=[" << strerror(errno) << "]"
        //         << std::endl;
        exit(1);
    }
    else {
        close(input_pipe[0]);
        close(output_pipe[1]);

        (void)keep_stdin_open;

        client.cgi_pid        = pid;
        client.cgi_output_fd  = output_pipe[0];
        client.cgi_input_fd   = input_pipe[1];
        client.is_cgi         = true;
    }
}

