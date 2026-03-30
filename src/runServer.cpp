#include <poll.h>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <set>
#include <sstream>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctime>
#include <csignal>
#include <cstdlib>  // rand()
#include <sstream>
#include "../includes/Server.hpp"
#include "../includes/ConfigParser.hpp"
#include "../includes/Client.hpp"
#include "../includes/Http.hpp"
#include "../includes/CGI.hpp"

// 全局 session 存储
std::map<std::string, std::string> sessions;  // session_id → username

std::string generateSessionId() {
    std::ostringstream oss;
    for (int i = 0; i < 16; i++)
        oss << std::hex << (rand() % 16);
    return oss.str();
}

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

            if (difftime(time(NULL), c.cgi_last_activity) > 10) {

                std::cerr << "now=" << time(NULL) 
              << " last=" << c.cgi_last_activity 
              << " diff=" << difftime(time(NULL), c.cgi_last_activity) << std::endl;

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
            if (!(fds[i].revents & POLLIN) && !(fds[i].revents & POLLHUP))
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

                // 收到请求头就先检查 Content-Length
                if (rbuf.find("\r\n\r\n") != std::string::npos) {
                    size_t cl_pos = rbuf.find("Content-Length: ");
                    if (cl_pos != std::string::npos) {
                        size_t cl_end = rbuf.find("\r\n", cl_pos);
                        size_t content_length = atoi(rbuf.substr(cl_pos + 16, cl_end - cl_pos - 16).c_str());
                        
                        // ✅ 只看 Content-Length 就够了，不用等 body 收完
                        if (content_length > clients[fds[i].fd].config->max_body) {
                            std::string resp = buildErrorResponse(413, *clients[fds[i].fd].config);
                            send(fds[i].fd, resp.c_str(), resp.size(), 0);
                            close(fds[i].fd);
                            clients.erase(fds[i].fd);
                            fds.erase(fds.begin() + i);
                            i--;
                            continue;
                        }
                    }
                }

                // 判断请求是否完整
                if (rbuf.find("\r\n\r\n") == std::string::npos)
                    continue;

                bool request_complete = false;
                if (rbuf.find("Transfer-Encoding: chunked") != std::string::npos) {
                    if (rbuf.find("0\r\n\r\n") != std::string::npos)
                        request_complete = true;
                } 
                else {
                    // ✅ 检查 body 是否完整收到
                    size_t header_end = rbuf.find("\r\n\r\n");
                    size_t cl_pos = rbuf.find("Content-Length: ");
                    if (cl_pos == std::string::npos) {
                        request_complete = true;  // 没有 body
                    } 
                    else {
                        size_t cl_end = rbuf.find("\r\n", cl_pos);
                        size_t content_length = atoi(rbuf.substr(cl_pos + 16, cl_end - cl_pos - 16).c_str());
                        size_t body_received = rbuf.size() - (header_end + 4);
                        if (body_received >= content_length)
                            request_complete = true;
                    }
                }

                if (!request_complete)
                    continue;

                // 解析请求
                HttpRequest req = parseRequest(rbuf);

                std::cerr << "body size: " << req.body.size() << std::endl;

                // 413 body 过大
                if (req.body.size() > clients[fds[i].fd].config->max_body) {
                    std::string resp = "HTTP/1.1 413 Content Too Large\r\n"
                                       "Content-Length: 0\r\n\r\n";
                    send(fds[i].fd, resp.c_str(), resp.size(), 0);
                    close(fds[i].fd);         
                    clients.erase(fds[i].fd);             
                    fds.erase(fds.begin() + i);                 
                    i--;  
                    continue;
                }

                // Session 路由
                if (req.path == "/login" && req.method == "GET") {
                    std::string body = "<html><body>"
                                    "<form method='POST' action='/login'>"
                                    "用户名: <input name='username' type='text'/>"
                                    "<input type='submit' value='登录'/>"
                                    "</form></body></html>";
                    std::ostringstream len;
                    len << body.size();
                    std::string resp = "HTTP/1.1 200 OK\r\n"
                                    "Content-Type: text/html; charset=utf-8\r\n"
                                    "Content-Length: " + len.str() + "\r\n"
                                    "\r\n" + body;
                    send(fds[i].fd, resp.c_str(), resp.size(), 0);
                    clients[fds[i].fd].recv_buffer.clear();
                    continue;
                }

                if (req.path == "/login" && req.method == "POST") {
                    // 从 body 里解析 username=Alice
                    std::string username = "";
                    std::string body = req.body;
                    size_t pos = body.find("username=");
                    if (pos != std::string::npos)
                        username = body.substr(pos + 9);

                    // 生成 session_id，存进 map
                    std::string sid = generateSessionId();
                    sessions[sid] = username;

                    std::string resp_body = "<html><body><h1>登录成功！</h1>"
                                            "<a href='/welcome'>进入欢迎页</a>"
                                            "</body></html>";
                    std::ostringstream len;
                    len << resp_body.size();
                    std::string resp = "HTTP/1.1 200 OK\r\n"
                                    "Content-Type: text/html; charset=utf-8\r\n"
                                    "Set-Cookie: sid=" + sid + "; Path=/\r\n"
                                    "Content-Length: " + len.str() + "\r\n"
                                    "\r\n" + resp_body;
                    send(fds[i].fd, resp.c_str(), resp.size(), 0);
                    clients[fds[i].fd].recv_buffer.clear();
                    continue;
                }

                if (req.path == "/welcome") {
                    // 从请求头里读 Cookie
                    std::string username = "陌生人";
                    if (req.headers.count("Cookie")) {
                        std::string cookie = req.headers["Cookie"];
                        size_t pos = cookie.find("sid=");
                        if (pos != std::string::npos) {
                            std::string sid = cookie.substr(pos + 4);
                            if (sessions.count(sid))
                                username = sessions[sid];
                        }
                    }
                    std::string body = "<html><body><h1>欢迎回来，" + username + "！</h1>"
                                    "<a href='/logout'>退出登录</a>"
                                    "</body></html>";
                    std::ostringstream len;
                    len << body.size();
                    std::string resp = "HTTP/1.1 200 OK\r\n"
                                    "Content-Type: text/html; charset=utf-8\r\n"
                                    "Content-Length: " + len.str() + "\r\n"
                                    "\r\n" + body;
                    send(fds[i].fd, resp.c_str(), resp.size(), 0);
                    clients[fds[i].fd].recv_buffer.clear();
                    continue;
                }

                if (req.path == "/logout") {
                    // 读 Cookie，删除 session
                    if (req.headers.count("Cookie")) {
                        std::string cookie = req.headers["Cookie"];
                        size_t pos = cookie.find("sid=");
                        if (pos != std::string::npos) {
                            std::string sid = cookie.substr(pos + 4);
                            sessions.erase(sid);
                        }
                    }
                    std::string body = "<html><body><h1>已退出登录</h1>"
                                    "<a href='/login'>重新登录</a>"
                                    "</body></html>";
                    std::ostringstream len;
                    len << body.size();
                    // Set-Cookie 过期时间设为过去，让浏览器删除 Cookie
                    std::string resp = "HTTP/1.1 200 OK\r\n"
                                    "Content-Type: text/html; charset=utf-8\r\n"
                                    "Set-Cookie: sid=; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n"
                                    "Content-Length: " + len.str() + "\r\n"
                                    "\r\n" + body;
                    send(fds[i].fd, resp.c_str(), resp.size(), 0);
                    clients[fds[i].fd].recv_buffer.clear();
                    continue;
                }

                // 路由判断 匹配 location
                LocationConfig* loc = matchLocation(*clients[fds[i].fd].config, req.path);
                if (!loc) {
                    std::string resp = "HTTP/1.1 404 Not Found\r\n"
                                       "Content-Length: 0\r\n\r\n";
                    send(fds[i].fd, resp.c_str(), resp.size(), 0);
                    clients[fds[i].fd].recv_buffer.clear();
                    continue;
                }

                if (loc->redirect_code != 0 && !loc->redirect_url.empty()) {
                    std::cerr << "redirect: " << loc->redirect_code << " → " << loc->redirect_url << std::endl;
                    std::ostringstream oss;
                    oss << loc->redirect_code;
                    std::string status_text = (loc->redirect_code == 301) ? "Moved Permanently" : "Found";
                    std::string resp = "HTTP/1.1 " + oss.str() + " " + status_text + "\r\n"
                                    "Location: " + loc->redirect_url + "\r\n"
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

                    int cgi_flags = fcntl(clients[fds[i].fd].cgi_output_fd, F_GETFL, 0);
                    fcntl(clients[fds[i].fd].cgi_output_fd, F_SETFL, cgi_flags | O_NONBLOCK);

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
