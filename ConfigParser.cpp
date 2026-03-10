#include "webserv.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t;");
    return s.substr(start, end - start + 1);
}

ServerConfig parseServer(std::ifstream &file) {
    
}

ServerConfig    parseConfig(const std::string &filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        std::cerr << "Error: cannot open " << filename << std::endl;
        exit(1);
    }

    std::string line;

    while (std::getline(file, line)) {
        line = trim(line);

        if (line.empty() || line[0] == '#')
            continue ;
        
        if (line == "server {") {
            return parseServer(file);
        }
    }
    file.close();
    return ServerConfig();
}

int main() {
    // if (argc != 2) {
    //     std::cerr << "Usage: ./webserv config.conf" << std::endl;
    //     return 1;
    // }
    // ServerConfig config = parseConfig(argv[1]);
    std::cout << trim("            listen 8080;   ") << std::endl;
    return 0;
}