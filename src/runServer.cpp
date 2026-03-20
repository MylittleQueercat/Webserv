#include <poll.h>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <set>
#include <sstream>
#include <sys/wait.h>
#include "../includes/Server.hpp"
#include "../includes/ConfigParser.hpp"
#include "../includes/Client.hpp"
#include "../includes/Http.hpp"
#include "../includes/CGI.hpp"

ClientState::ClientState() : fd(-1), headers_done(false), 
                              content_length(0), config(NULL),
                              cgi_pid(-1), cgi_output_fd(-1), is_cgi(false){}

void runServer(std::vector<ServerConfig> &configs) {
    std::vector<struct pollfd> fds;
    std::set<int> server_fds;
    std::map<int, ClientState> clients;

    // 第一步：把所有 server_fd 放进 fds
    for (size_t i = 0; i < configs.size(); i++) {
        struct pollfd pfd;
        pfd.fd      = configs[i].server_fd;
        pfd.events  = POLLIN;
        pfd.revents = 0;
        fds.push_back(pfd);
        server_fds.insert(configs[i].server_fd);
    }

    // 第二步：事件循环
    while (true) {
        int ready = poll(&fds[0], fds.size(), -1);
        if (ready < 0) {
            std::cerr << "poll() failed" << std::endl;
            break;
        }

        // 第三步：遍历所有 fd
        for (size_t i = 0; i < fds.size(); i++) {
            if (!(fds[i].revents & POLLIN))
                continue;

            // 检查是不是 CGI pipe 的 fd
            bool is_cgi_fd = false;
            int cgi_client_fd = -1;
            for (std::map<int, ClientState>::iterator it = clients.begin(); 
                it != clients.end(); ++it) {
                if (it->second.cgi_output_fd == fds[i].fd) {
                    is_cgi_fd = true;
                    cgi_client_fd = it->second.fd;
                    break;
                }
            }

            if (is_cgi_fd) {
                // 读取 CGI 输出
                std::string output;
                char buffer[4096];
                int bytes;
                while ((bytes = read(fds[i].fd, buffer, sizeof(buffer))) > 0)
                    output += std::string(buffer, bytes);
                close(fds[i].fd);
                fds.erase(fds.begin() + i);
                i--;

                // 等次进程结束
                waitpid(clients[cgi_client_fd].cgi_pid, NULL, 0);
                clients[cgi_client_fd].is_cgi = false;

                // 构建响应
                std::string body_only = output.substr(output.find("\r\n\r\n") + 4);
                std::string headers_only = output.substr(0, output.find("\r\n\r\n"));
                std::ostringstream oss;
                oss << body_only.size();

                std::string response = "HTTP/1.1 200 OK\r\n";
                response += "Connection: close\r\n";
                response += headers_only + "\r\n";
                response += "Content-Length: " + oss.str() + "\r\n";
                response += "\r\n";
                response += body_only;

                send(cgi_client_fd, response.c_str(), response.size(), 0);
                continue;
            }

            if (server_fds.count(fds[i].fd)) {
                // 新客户端连进来
                int client_fd = accept(fds[i].fd, NULL, NULL);
                int flags = fcntl(client_fd, F_GETFL, 0);
                fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

                struct pollfd client_pfd;
                client_pfd.fd      = client_fd;
                client_pfd.events  = POLLIN;
                client_pfd.revents = 0;
                fds.push_back(client_pfd);

                ClientState state;
                state.fd = client_fd;
                state.config = &configs[0];
                clients[client_fd] = state; 

                std::cout << "新客户端连接 fd=" << client_fd << std::endl;

            } else {
                // 客户端发数据了
                char buffer[4096];
                int bytes = recv(fds[i].fd, buffer, sizeof(buffer), 0);

                if (bytes <= 0) {
                    std::cout << "客户端断开 fd=" << fds[i].fd << std::endl;
                    close(fds[i].fd);
                    clients.erase(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--;
                }
                else {
                    clients[fds[i].fd].recv_buffer += std::string(buffer, bytes);
                    
                    // Get client's recv_buffer
                    std::string &buf = clients[fds[i].fd].recv_buffer;
                    // Check if request is full
                    bool request_complete = false;
                    if (buf.find("\r\n\r\n") != std::string::npos) {
                        if (buf.find("Transfer-Encoding: chunked") != std::string::npos) {
                            if (buf.find("0\r\n\r\n") != std::string::npos)
                                request_complete = true;
                        }
                        else
                            request_complete = true;
                            //一直等待request完整再继续

                        if (request_complete) {
                            //Parse raw buffer into HttpRequest struct
                            HttpRequest req = parseRequest(buf);

                            // Check if body exceeds max_body -> if yes return 413
                            if (req.body.size() > clients[fds[i].fd].config->max_body) {
                                std::string response = 
                                    "HTTP/1.1 413 Content TOO Large\r\n"
                                    "Content-Length: 0\r\n"
                                    "\r\n";
                                send(fds[i].fd, response.c_str(), response.size(), 0);
                                clients[fds[i].fd].recv_buffer.clear();
                                continue;
                            }

                            //用这个客户端的服务器配置和请求路径，去找最匹配的 location
                            LocationConfig* loc = matchLocation(*clients[fds[i].fd].config, req.path);
                            if (!loc) {
                                std::string response = "HTTP/1.1 404 Not Found\r\n"
                                                        "Content-Length: 0\r\n\r\n";
                                send(fds[i].fd, response.c_str(), response.size(), 0);
                                clients[fds[i].fd].recv_buffer.clear();
                                continue;
                            }

                            //405检查(路由匹配失败/方法不允许就返回405)
                            bool method_allowed = false;
                            for (size_t j = 0; j < loc->methods.size(); j++) {
                                if (loc->methods[j] == req.method)
                                    method_allowed = true;
                            }
                            if (!method_allowed) {
                                std::string response = "HTTP/1.1 405 Method Not Allowed\r\n"
                                                        "Content-Length: 0\r\n\r\n";
                                send(fds[i].fd, response.c_str(), response.size(), 0);
                                clients[fds[i].fd].recv_buffer.clear();
                                continue;
                            }

                            // 检查是否是 CGI 请求
                            if (!loc->cgi_ext.empty() && 
                                req.path.find(loc->cgi_ext) != std::string::npos) {
                                // std::string response = executeCGI(req, *loc);
                                // send(fds[i].fd, response.c_str(), response.size(), 0);
                                startCGI(req, *loc, clients[fds[i].fd]);
                                struct pollfd cgi_pfd;
                                cgi_pfd.fd      = clients[fds[i].fd].cgi_output_fd;
                                cgi_pfd.events  = POLLIN;
                                cgi_pfd.revents = 0;
                                fds.push_back(cgi_pfd);
                                clients[fds[i].fd].recv_buffer.clear();
                                continue;
                            }
                            std::cout << "method: " << req.method << std::endl;
                            std::cout << "path: " << req.path << std::endl;
                            std::cout << "body: " << req.body << std::endl;

                            // 找到空行！请求头部完整了
                            std::cout << "收到完整请求：" << std::endl;
                            std::cout << buf << std::endl;
                            
                            // 返回HTTP 响应
                            std::string response = handleRequest(req);
                                
                            send(fds[i].fd, response.c_str(), response.size(), 0);
                            
                            // clear buffer after processing
                            clients[fds[i].fd].recv_buffer.clear();
                        }
                    }
                }
            }
        }
    }
}

