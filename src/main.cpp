#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <vector>
#include <iostream>

#include "Server.hpp"
#include "ConfigParser.hpp"
#include "Webserv.hpp"
#include "Http.hpp"

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./webserv config.conf" << std::endl;
        return 1;
    }

    // 1. 解析配置文件
    ServerConfig config = parseConfig(argv[1]);

    // 2. 初始化 server socket
    Server server;
    if (!server.setup("0.0.0.0", config.port)) {
        std::cerr << "Error: server setup failed" << std::endl;
        return 1;
    }
    config.server_fd = server.get_fd();

    // 测试路由器
    LocationConfig* loc = matchLocation(config, "/upload/photo.jpg");
    if (loc)
        std::cout << "matched: " << loc->path << std::endl;
    else
        std::cout << "no match" << std::endl;

    // 3. 启动 poll() 循环
    std::vector<ServerConfig> configs;
    configs.push_back(config);
    runServer(configs);
    
    return 0;
}

