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

ClientState::ClientState() : fd(-1), headers_done(false), 
                              content_length(0), config(NULL) {}

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
                    if (buf.find("\r\n\r\n") != std::string::npos) {
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

                        std::cout << "method: " << req.method << std::endl;
                        std::cout << "path: " << req.path << std::endl;
                        std::cout << "body: " << req.body << std::endl;

                        // 找到空行！请求头部完整了
                        std::cout << "收到完整请求：" << std::endl;
                        std::cout << buf << std::endl;
                        
                        // 返回一个简单的 HTTP 响应
                        std::string response =
                            "HTTP/1.1 200 OK\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 13\r\n"
                            "\r\n"
                            "Hello World!\n";
                        send(fds[i].fd, response.c_str(), response.size(), 0);
                        
                        // clear buffer after processing
                        clients[fds[i].fd].recv_buffer.clear();
                    }
                }
            }
        }
    }
}

