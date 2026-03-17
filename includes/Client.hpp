#ifndef CLIENT
# define CLIENT

# include <string>

struct ClientState {
    int         fd;
    std::string recv_buffer;  // 累积收到的数据
    std::string send_buffer;  // 等待发送的数据
    bool        headers_done; // 头部读完了吗？
    size_t      content_length; // 请求体有多长

    ClientState();
};

#endif