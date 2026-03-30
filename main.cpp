#include "includes/Webserv.hpp"

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./webserv config.conf" << std::endl;
        return 1;
    }

    // 1. Parse all server{} blocks
    std::vector<ServerConfig> configs = parseConfigs(argv[1]);
    if (configs.empty()) {
        std::cerr << "Error: no server config found" << std::endl;
        return 1;
    }

    // 2. Start a Server for each ServerConfig
    std::vector<Server*> servers;
    for (size_t i = 0; i < configs.size(); i++) {
        Server* server = new Server();
        if (!server->setup("0.0.0.0", configs[i].port)) {
            std::cerr << "Error: server setup failed on port "
                      << configs[i].port << std::endl;
            delete server;
            continue;
        }
        configs[i].server_fd = server->get_fd();
        servers.push_back(server);
    }

    if (servers.empty()) {
        std::cerr << "Error: no server could be started" << std::endl;
        return 1;
    }

    // Start poll() event loop
    runServer(configs);

    // Cleanup
    for (size_t i = 0; i < servers.size(); i++)
        delete servers[i];

    return 0;
}
