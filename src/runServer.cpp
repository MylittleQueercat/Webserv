#include <poll.h>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <set>
#include "../includes/Server.hpp"
#include "../includes/ConfigParser.hpp"

void runServer(std::vector<ServerConfig> &configs) {
    std::vector<struct pollfd> fds;
    std::set<int> server_fds;

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
                std::cout << "新客户端连接 fd=" << client_fd << std::endl;

            } else {
                // 客户端发数据了
                char buffer[4096];
                int bytes = recv(fds[i].fd, buffer, sizeof(buffer), 0);

                if (bytes <= 0) {
                    std::cout << "客户端断开 fd=" << fds[i].fd << std::endl;
                    close(fds[i].fd);
                    fds.erase(fds.begin() + i);
                    i--;
                } else {
                    // 暂时 echo 回去
                    send(fds[i].fd, buffer, bytes, 0);
                }
            }
        }
    }
}