// #include <poll.h>
// #include <vector>
// #include <iostream>
// #include <unistd.h>
// #include <fstream>
// #include <set>
// #include <sstream>
// #include <sys/wait.h>
// #include "../includes/Server.hpp"
// #include "../includes/ConfigParser.hpp"
// #include "../includes/Client.hpp"
// #include "../includes/Http.hpp"
// #include "../includes/CGI.hpp"

// ClientState::ClientState() : fd(-1), headers_done(false), 
//                               content_length(0), config(NULL),
//                               cgi_pid(-1), cgi_output_fd(-1), is_cgi(false){}

// void runServer(std::vector<ServerConfig> &configs) {
//     std::vector<struct pollfd> fds;
//     std::set<int> server_fds;
//     std::map<int, ClientState> clients;
    
//     //server_fd → ServerConfig* 的映射
//     std::map<int, ServerConfig*> fd_to_config;

//     for (size_t i = 0; i < configs.size(); i++) {
//         struct pollfd pfd;
//         pfd.fd      = configs[i].server_fd;
//         pfd.events  = POLLIN;
//         pfd.revents = 0;
//         fds.push_back(pfd);
//         server_fds.insert(configs[i].server_fd);
        
//         // ✅ 新增：记录映射关系
//         fd_to_config[configs[i].server_fd] = &configs[i];
//     }
//     // 第一步：把所有 server_fd 放进 fds
//     for (size_t i = 0; i < configs.size(); i++) {
//         struct pollfd pfd;
//         pfd.fd      = configs[i].server_fd;
//         pfd.events  = POLLIN;
//         pfd.revents = 0;
//         fds.push_back(pfd);
//         server_fds.insert(configs[i].server_fd);
//     }

//     // 第二步：事件循环
//     while (true) {
//         int ready = poll(&fds[0], fds.size(), -1);
//         if (ready < 0) {
//             std::cerr << "poll() failed" << std::endl;
//             break;
//         }

//         // 第三步：遍历所有 fd
//         for (size_t i = 0; i < fds.size(); i++) {
//             if (!(fds[i].revents & POLLIN))
//             //如果没有事件发生就继续监听
//                 continue;

//             // 检查是不是 CGI pipe 的 fd
//             bool is_cgi_fd = false;
//             int cgi_client_fd = -1;
//             for (std::map<int, ClientState>::iterator it = clients.begin(); 
//                 it != clients.end(); ++it) {
//                 if (it->second.cgi_output_fd == fds[i].fd) {
//                     is_cgi_fd = true;
//                     cgi_client_fd = it->second.fd;
//                     break;
//                 }
//             }
 
//             if (is_cgi_fd) {
//                 // 读取 CGI 输出
//                 std::string output;
//                 char buffer[4096];
//                 int bytes;
//                 while ((bytes = read(fds[i].fd, buffer, sizeof(buffer))) > 0)
//                     output += std::string(buffer, bytes);
//                 close(fds[i].fd);
//                 fds.erase(fds.begin() + i);
//                 i--;

//                 // 等次进程结束
//                 waitpid(clients[cgi_client_fd].cgi_pid, NULL, 0);
//                 clients[cgi_client_fd].is_cgi = false;

//                 // 构建响应
//                 std::string body_only = output.substr(output.find("\r\n\r\n") + 4);
//                 std::string headers_only = output.substr(0, output.find("\r\n\r\n"));
//                 std::ostringstream oss;
//                 oss << body_only.size();

//                 std::string response = "HTTP/1.1 200 OK\r\n";
//                 response += "Connection: close\r\n";
//                 response += headers_only + "\r\n";
//                 response += "Content-Length: " + oss.str() + "\r\n";
//                 response += "\r\n";
//                 response += body_only;

//                 send(cgi_client_fd, response.c_str(), response.size(), 0);
//                 continue;
//             }

//             if (server_fds.count(fds[i].fd)) {
//                 // 新客户端连进来
//                 int client_fd = accept(fds[i].fd, NULL, NULL);
//                 int flags = fcntl(client_fd, F_GETFL, 0);
//                 fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

//                 struct pollfd client_pfd;
//                 client_pfd.fd      = client_fd;
//                 client_pfd.events  = POLLIN;
//                 client_pfd.revents = 0;
//                 fds.push_back(client_pfd);

//                 ClientState state;
//                 state.fd = client_fd;
//                 state.config = fd_to_config[fds[i].fd];
//                 //state.config = &configs[0];
//                 clients[client_fd] = state; 
//                 std::cout << "新客户端连接 fd=" << client_fd 
//                             << " → port=" << state.config->port << std::endl;
//                 //std::cout << "新客户端连接 fd=" << client_fd << std::endl;

//             } else {
//                 // 客户端发数据了
//                 char buffer[4096];
//                 int bytes = recv(fds[i].fd, buffer, sizeof(buffer), 0);

//                 if (bytes <= 0) {
//                     std::cout << "客户端断开 fd=" << fds[i].fd << std::endl;
//                     close(fds[i].fd);
//                     clients.erase(fds[i].fd);
//                     fds.erase(fds.begin() + i);
//                     i--;
//                 }
//                 else {
//                     clients[fds[i].fd].recv_buffer += std::string(buffer, bytes);
                    
//                     // Get client's recv_buffer
//                     std::string &buf = clients[fds[i].fd].recv_buffer;
//                     // Check if request is full
//                     bool request_complete = false;
//                     if (buf.find("\r\n\r\n") != std::string::npos) {
//                         if (buf.find("Transfer-Encoding: chunked") != std::string::npos) {
//                             if (buf.find("0\r\n\r\n") != std::string::npos)
//                                 request_complete = true;
//                         }
//                         else
//                             request_complete = true;
//                             //一直等待request完整再继续

//                         if (request_complete) {
//                             //Parse raw buffer into HttpRequest struct
//                             HttpRequest req = parseRequest(buf);

//                             // Check if body exceeds max_body -> if yes return 413
//                             if (req.body.size() > clients[fds[i].fd].config->max_body) {
//                                 std::string response = 
//                                     "HTTP/1.1 413 Content TOO Large\r\n"
//                                     "Content-Length: 0\r\n"
//                                     "\r\n";
//                                 send(fds[i].fd, response.c_str(), response.size(), 0);
//                                 clients[fds[i].fd].recv_buffer.clear();
//                                 continue;
//                             }

//                             //用这个客户端的服务器配置和请求路径，去找最匹配的 location
//                             LocationConfig* loc = matchLocation(*clients[fds[i].fd].config, req.path);
//                             if (!loc) {
//                                 std::string response = "HTTP/1.1 404 Not Found\r\n"
//                                                         "Content-Length: 0\r\n\r\n";
//                                 send(fds[i].fd, response.c_str(), response.size(), 0);
//                                 clients[fds[i].fd].recv_buffer.clear();
//                                 continue;
//                             }

//                             //405检查(路由匹配失败/方法不允许就返回405)
//                             bool method_allowed = false;
//                             for (size_t j = 0; j < loc->methods.size(); j++) {
//                                 if (loc->methods[j] == req.method)
//                                     method_allowed = true;
//                             }
//         //                     if (!method_allowed) {
//         //                         std::string response = buildErrorResponse(405, 
//         // clients[fds[i].fd].config->error_page);
//         //                         send(fds[i].fd, response.c_str(), response.size(), 0);
//         //                         clients[fds[i].fd].recv_buffer.clear();
//         //                         continue;
//         //                     }
//                             if (!method_allowed) {
//                                 std::string response = buildErrorResponse(405, *clients[fds[i].fd].config);
//                                 send(fds[i].fd, response.c_str(), response.size(), 0);
//                                 clients[fds[i].fd].recv_buffer.clear();
//                                 continue;
//                             }
//                             // if (!method_allowed) {
//                             //     std::cout << "DEBUG 405: method=" << req.method 
//                             //             << " path=" << req.path << std::endl;
//                             //     std::cout << "DEBUG loc methods: ";
//                             //     for (size_t j = 0; j < loc->methods.size(); j++)
//                             //         std::cout << "[" << loc->methods[j] << "] ";
//                             //     std::cout << std::endl;
//                             // }                                    
//                             // 检查是否是 CGI 请求
//                             if (!loc->cgi_ext.empty() && 
//                                 req.path.find(loc->cgi_ext) != std::string::npos) {
//                                 // std::string response = executeCGI(req, *loc);
//                                 // send(fds[i].fd, response.c_str(), response.size(), 0);
//                                 startCGI(req, *loc, clients[fds[i].fd]);
//                                 struct pollfd cgi_pfd;
//                                 cgi_pfd.fd      = clients[fds[i].fd].cgi_output_fd;
//                                 cgi_pfd.events  = POLLIN;
//                                 cgi_pfd.revents = 0;
//                                 fds.push_back(cgi_pfd);
//                                 clients[fds[i].fd].recv_buffer.clear();
//                                 continue;
//                             }
//                             std::cout << "method: " << req.method << std::endl;
//                             std::cout << "path: " << req.path << std::endl;
//                             std::cout << "body: " << req.body << std::endl;

//                             // 找到空行！请求头部完整了
//                             std::cout << "收到完整请求：" << std::endl;
//                             std::cout << buf << std::endl;
                            
//                             // 返回HTTP 响应
//                             std::string response = handleRequest(req, *clients[fds[i].fd].config, *loc);
                                
//                             send(fds[i].fd, response.c_str(), response.size(), 0);
                            
//                             // clear buffer after processing
//                             clients[fds[i].fd].recv_buffer.clear();
//                         }
//                     }
//                 }
//             }
//         }
//     }
// }

#include <poll.h>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <set>
#include <sstream>
#include <sys/wait.h>
#include <fcntl.h>
#include <cerrno>
#include <ctime>
#include <csignal>
#include "../includes/Server.hpp"
#include "../includes/ConfigParser.hpp"
#include "../includes/Client.hpp"
#include "../includes/Http.hpp"
#include "../includes/CGI.hpp"

ClientState::ClientState() : fd(-1), headers_done(false),
                              content_length(0), config(NULL),
                              cgi_pid(-1), cgi_output_fd(-1), is_cgi(false),
                              cgi_last_activity(0) {}

void runServer(std::vector<ServerConfig> &configs) {
    std::vector<struct pollfd> fds;
    std::set<int> server_fds;
    std::map<int, ClientState> clients;
    std::map<int, ServerConfig*> fd_to_config;

    // ── 第一步：注册所有 server_fd ──────────────────────────────────────
    for (size_t i = 0; i < configs.size(); i++) {
        struct pollfd pfd;
        pfd.fd      = configs[i].server_fd;
        pfd.events  = POLLIN;
        pfd.revents = 0;
        fds.push_back(pfd);
        server_fds.insert(configs[i].server_fd);
        fd_to_config[configs[i].server_fd] = &configs[i];
    }

    // ── 第二步：事件循环 ────────────────────────────────────────────────
    while (true) {

        // poll() 最多等 1 秒，保证超时检查每秒都能跑到
        int ready = poll(&fds[0], fds.size(), 1000);
        if (ready < 0) {
            std::cerr << "poll() failed" << std::endl;
            break;
        }

        // ── 超时检查：遍历所有 CGI 客户端 ────────────────────────────────
        for (std::map<int, ClientState>::iterator it = clients.begin();
             it != clients.end(); ++it) {
            ClientState& c = it->second;
            if (!c.is_cgi) continue;
            if (c.cgi_last_activity == 0) continue;

            if (difftime(time(NULL), c.cgi_last_activity) > 60) {
                std::cerr << "CGI timeout: pid=" << c.cgi_pid << std::endl;

                kill(c.cgi_pid, SIGKILL);
                waitpid(c.cgi_pid, NULL, WNOHANG);

                // 从 fds 里删掉 cgi_output_fd
                for (size_t j = 0; j < fds.size(); j++) {
                    if (fds[j].fd == c.cgi_output_fd) {
                        close(fds[j].fd);
                        fds.erase(fds.begin() + j);
                        break;
                    }
                }

                c.cgi_output_fd = -1;
                c.is_cgi        = false;

                std::string resp = "HTTP/1.1 504 Gateway Timeout\r\n"
                                   "Content-Length: 0\r\n\r\n";
                send(c.fd, resp.c_str(), resp.size(), 0);
            }
        }

        // ── 遍历所有 fd ───────────────────────────────────────────────────
        for (size_t i = 0; i < fds.size(); i++) {
            if (!(fds[i].revents & POLLIN))
                continue;

            // ── 判断是否是 CGI pipe fd ────────────────────────────────────
            bool         is_cgi_fd  = false;
            ClientState* cgi_client = NULL;

            for (std::map<int, ClientState>::iterator it = clients.begin();
                 it != clients.end(); ++it) {
                if (it->second.cgi_output_fd == fds[i].fd) {
                    is_cgi_fd  = true;
                    cgi_client = &it->second;
                    break;
                }
            }

            // ── 处理 CGI pipe 可读 ────────────────────────────────────────
            if (is_cgi_fd) {
                char buf[4096];
                int  bytes = read(fds[i].fd, buf, sizeof(buf));

                if (bytes > 0) {
                    // 累积数据，更新活动时间
                    cgi_client->cgi_output += std::string(buf, bytes);
                    cgi_client->cgi_last_activity = time(NULL);
                }
                else if (bytes == 0) {
                    // pipe 关闭 = CGI 进程退出 = 数据读完
                    close(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--;

                    waitpid(cgi_client->cgi_pid, NULL, WNOHANG);
                    cgi_client->is_cgi       = false;
                    cgi_client->cgi_output_fd = -1;

                    // 构建 HTTP 响应
                    std::string& output = cgi_client->cgi_output;
                    std::string response;

                    size_t header_end = output.find("\r\n\r\n");
                    if (header_end != std::string::npos) {
                        std::string cgi_headers = output.substr(0, header_end);
                        std::string body        = output.substr(header_end + 4);
                        std::ostringstream oss;
                        oss << body.size();

                        response  = "HTTP/1.1 200 OK\r\n";
                        response += "Connection: close\r\n";
                        response += cgi_headers + "\r\n";
                        response += "Content-Length: " + oss.str() + "\r\n\r\n";
                        response += body;
                    } else {
                        // CGI 没有输出头部，直接当 body
                        std::ostringstream oss;
                        oss << output.size();
                        response  = "HTTP/1.1 200 OK\r\n";
                        response += "Connection: close\r\n";
                        response += "Content-Length: " + oss.str() + "\r\n\r\n";
                        response += output;
                    }

                    send(cgi_client->fd, response.c_str(), response.size(), 0);
                    cgi_client->cgi_output.clear();
                }
                else {
                    // bytes < 0
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // 非阻塞暂时没数据，等下一轮
                    }
                    else {
                        // 真正的读取错误
                        close(fds[i].fd);
                        fds.erase(fds.begin() + i);
                        i--;

                        kill(cgi_client->cgi_pid, SIGKILL);
                        waitpid(cgi_client->cgi_pid, NULL, WNOHANG);
                        cgi_client->is_cgi        = false;
                        cgi_client->cgi_output_fd = -1;

                        std::string err = "HTTP/1.1 500 Internal Server Error\r\n"
                                          "Content-Length: 0\r\n\r\n";
                        send(cgi_client->fd, err.c_str(), err.size(), 0);
                    }
                }
                continue; // CGI 处理完，跳过后面的逻辑
            }

            // ── 新客户端连接 ──────────────────────────────────────────────
            if (server_fds.count(fds[i].fd)) {
                int client_fd = accept(fds[i].fd, NULL, NULL);
                if (client_fd < 0) {
                    std::cerr << "accept() failed" << std::endl;
                    continue;
                }

                int flags = fcntl(client_fd, F_GETFL, 0);
                fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

                struct pollfd client_pfd;
                client_pfd.fd      = client_fd;
                client_pfd.events  = POLLIN;
                client_pfd.revents = 0;
                fds.push_back(client_pfd);

                ClientState state;
                state.fd     = client_fd;
                state.config = fd_to_config[fds[i].fd];
                clients[client_fd] = state;

                std::cout << "新客户端连接 fd=" << client_fd
                          << " → port=" << state.config->port << std::endl;
                continue;
            }

            // ── 普通客户端发数据 ──────────────────────────────────────────
            {
                char buf[4096];
                int  bytes = recv(fds[i].fd, buf, sizeof(buf), 0);

                if (bytes <= 0) {
                    std::cout << "客户端断开 fd=" << fds[i].fd << std::endl;
                    close(fds[i].fd);
                    clients.erase(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--;
                    continue;
                }

                clients[fds[i].fd].recv_buffer += std::string(buf, bytes);
                std::string& rbuf = clients[fds[i].fd].recv_buffer;

                // 判断请求是否完整
                if (rbuf.find("\r\n\r\n") == std::string::npos)
                    continue;

                bool request_complete = false;
                if (rbuf.find("Transfer-Encoding: chunked") != std::string::npos) {
                    if (rbuf.find("0\r\n\r\n") != std::string::npos)
                        request_complete = true;
                } else {
                    request_complete = true;
                }

                if (!request_complete)
                    continue;

                // 解析请求
                HttpRequest req = parseRequest(rbuf);

                // 413 body 过大
                if (req.body.size() > clients[fds[i].fd].config->max_body) {
                    std::string resp = "HTTP/1.1 413 Content Too Large\r\n"
                                       "Content-Length: 0\r\n\r\n";
                    send(fds[i].fd, resp.c_str(), resp.size(), 0);
                    clients[fds[i].fd].recv_buffer.clear();
                    continue;
                }

                // 匹配 location
                LocationConfig* loc = matchLocation(*clients[fds[i].fd].config, req.path);
                if (!loc) {
                    std::string resp = "HTTP/1.1 404 Not Found\r\n"
                                       "Content-Length: 0\r\n\r\n";
                    send(fds[i].fd, resp.c_str(), resp.size(), 0);
                    clients[fds[i].fd].recv_buffer.clear();
                    continue;
                }

                // 405 方法不允许
                bool method_allowed = false;
                for (size_t j = 0; j < loc->methods.size(); j++) {
                    if (loc->methods[j] == req.method)
                        method_allowed = true;
                }
                if (!method_allowed) {
                    std::string resp = buildErrorResponse(405, *clients[fds[i].fd].config);
                    send(fds[i].fd, resp.c_str(), resp.size(), 0);
                    clients[fds[i].fd].recv_buffer.clear();
                    continue;
                }

                // CGI 请求
                if (!loc->cgi_ext.empty() &&
                    req.path.find(loc->cgi_ext) != std::string::npos) {
                    startCGI(req, *loc, clients[fds[i].fd]);

                    // 把 cgi_output_fd 加进 poll 监听
                    struct pollfd cgi_pfd;
                    cgi_pfd.fd      = clients[fds[i].fd].cgi_output_fd;
                    cgi_pfd.events  = POLLIN;
                    cgi_pfd.revents = 0;
                    fds.push_back(cgi_pfd);

                    // 初始化超时计时
                    clients[fds[i].fd].cgi_last_activity = time(NULL);

                    clients[fds[i].fd].recv_buffer.clear();
                    continue;
                }

                // 普通请求
                std::cout << "method: " << req.method << std::endl;
                std::cout << "path:   " << req.path   << std::endl;
                std::cout << "body:   " << req.body   << std::endl;

                std::string resp = handleRequest(req, *clients[fds[i].fd].config, *loc);
                send(fds[i].fd, resp.c_str(), resp.size(), 0);
                clients[fds[i].fd].recv_buffer.clear();
            }
        }
    }
}
