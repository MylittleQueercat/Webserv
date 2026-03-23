#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <vector>
#include <iostream>

#include "includes/Server.hpp"
#include "includes/ConfigParser.hpp"
#include "includes/Webserv.hpp"
#include "includes/Http.hpp"

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./webserv config.conf" << std::endl;
        return 1;
    }

    // 1. 解析所有 server{}
    std::vector<ServerConfig> configs = parseConfigs(argv[1]);
    if (configs.empty()) {
        std::cerr << "Error: no server config found" << std::endl;
        return 1;
    }

    // 2. 为每个 ServerConfig 启动一个 Server
    std::vector<Server*> servers;
    for (size_t i = 0; i < configs.size(); i++) {
        Server* server = new Server();
        if (!server->setup("0.0.0.0", configs[i].port)) {
            std::cerr << "Error: server setup failed on port "
                    << configs[i].port << std::endl;
            delete server;
            return 1;
        }
        configs[i].server_fd = server->get_fd();
        servers.push_back(server);
    }

    // 启动 poll()
    runServer(configs);

    // 清理
    for (size_t i = 0; i < servers.size(); i++)
        delete servers[i];

    return 0;
}
