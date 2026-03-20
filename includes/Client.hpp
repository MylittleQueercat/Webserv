#ifndef CLIENT
# define CLIENT

# include <string>
# include "ConfigParser.hpp"

struct ClientState {
    int         fd;
    std::string recv_buffer;  // 累积收到的数据
    std::string send_buffer;  // 等待发送的数据
    bool        headers_done; // 头部读完了吗？
    size_t      content_length; // 请求体有多长
    ServerConfig *config; //可以帮助直接查找每个客户端具体连接哪个服务器

    pid_t   cgi_pid;
    int     cgi_output_fd;
    bool    is_cgi;

    ClientState();
};

#endif