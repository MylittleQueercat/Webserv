#ifndef CLIENT
# define CLIENT

# include <string>
# include <unistd.h>
# include "ConfigParser.hpp"
# include <ctime>

struct ClientState {
    int         fd;
    std::string recv_buffer;  // 累积收到的数据
    std::string send_buffer;  // 等待发送的数据
    bool        headers_done; // 头部读完了吗？
    size_t      content_length; // 请求体有多长
    ServerConfig *config; //可以帮助直接查找每个客户端具体连接哪个服务器
    
    //CGI
    pid_t   cgi_pid;
    int     cgi_output_fd;
    bool    is_cgi;
    std::string cgi_output;        // ← 新增
    time_t      cgi_last_activity; // ← 新增

    ClientState();
};

#endif