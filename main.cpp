#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <vector>

int main() {
    // 前面的部分你已经懂了
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 128);
    fcntl(server_fd, F_SETFL, O_NONBLOCK);

    // ---- 新增部分 ----

    // 用 vector 管理所有 fd
    std::vector<struct pollfd> fds;

    // 先把 server_fd 放进去
    struct pollfd server_pfd;
    server_pfd.fd     = server_fd;
    server_pfd.events = POLLIN;   // 关心：有新连接吗？
    fds.push_back(server_pfd);

    // 事件循环
    while (true) {
        // 问一遍：谁准备好了？
        poll(&fds[0], fds.size(), -1);
        //                         ↑ -1 表示永远等，直到有人准备好

        // 遍历所有 fd
        for (size_t i = 0; i < fds.size(); i++) {

            if (fds[i].revents & POLLIN) {

                if (fds[i].fd == server_fd) {
                    // 是 server_fd 可读 → 有新客户端连接！
                    int client_fd = accept(server_fd, NULL, NULL);
                    fcntl(client_fd, F_SETFL, O_NONBLOCK);

                    struct pollfd client_pfd;
                    client_pfd.fd     = client_fd;
                    client_pfd.events = POLLIN;
                    fds.push_back(client_pfd);  // 加入监听列表

                } else {
                    // 是某个 client_fd 可读 → 客户端发数据了！
                    char buffer[1024];
                    int bytes = recv(fds[i].fd, buffer, sizeof(buffer), 0);

                    if (bytes <= 0) {
                        // 客户端断开了
                        close(fds[i].fd);
                        fds.erase(fds.begin() + i);
                        i--;  // 注意：删了一个元素，i 要回退
                    } else {
                        // echo：原样发回去
                        send(fds[i].fd, buffer, bytes, 0);
                    }
                }
            }
        }
    }
}

