#include "../includes/ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>

std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t;");
    return s.substr(start, end - start + 1);
}

size_t  parseSize(const std::string &s) {
    size_t  multiplier = 1;
    if (s[s.size() - 1] == 'm')
        multiplier = 1024 * 1024;
    else if (s[s.size() - 1] == 'k')
        multiplier = 1024;
    return atoi(s.c_str()) * multiplier;
}

LocationConfig parseLocation(std::ifstream &file, const std::string &path) {
    LocationConfig loc;
    loc.path = path;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line == "}")
            break;

        std::istringstream iss(line);
        std::string key;
        iss >> key;
        if (key == "methods") {
            std::string method;
            while (iss >> method)
                loc.methods.push_back(trim(method));
        }
        else if (key == "autoindex") {
            std::string val;
            iss >> val;
            loc.autoindex = (trim(val) == "on");
        }
        else if (key == "index") {
            std::string val;
            iss >> val;
            loc.index = trim(val);
        }
        else if (key == "upload_store") {
            std::string val;
            iss >> val;
            loc.upload_store = trim(val);
        }
        else if (key == "cgi_ext") {
            std::string val;
            iss >> val;
            loc.cgi_ext = trim(val);
        }
    }
    return loc;
}

ServerConfig parseServer(std::ifstream &file) {
    ServerConfig    config;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        if (line == "}")
            break;
        if (line.substr(0, 8) == "location") {
            std::istringstream iss(line);
            std::string keyword, path, brace;
            iss >> keyword >> path >> brace;
            LocationConfig loc = parseLocation(file, path);
            config.locations.push_back(loc);
        }
        else {
            std::istringstream iss(line);
            std::string key;
            iss >> key;

            if (key == "listen") {
                std::string val;
                iss >> val;
                config.port = atoi(trim(val).c_str());
            }
            else if (key == "root") {
                std::string val;
                iss >> val;
                config.root = trim(val);
            }
            else if (key == "error_page") {
                std::string code, path;
                iss >> code >> path;
                config.error_page = trim(path);
            }
            else if (key == "client_max_body_size") {
                std::string val;
                iss >> val;
                config.max_body = parseSize(trim(val));
            }
        }
    }
    return config;
}

std::vector<ServerConfig> parseConfigs(const std::string &filename) {
    std::vector<ServerConfig> servers;
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        std::cerr << "Error: cannot open " << filename << std::endl;
        exit(1);
    }
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        if (line == "server {")
            servers.push_back(parseServer(file));  // 每个 server{} 加进去
    }
    file.close();
    return servers;
}

// ServerConfig    parseConfig(const std::string &filename) {
//     std::ifstream file(filename.c_str());
//     if (!file.is_open()) {
//         std::cerr << "Error: cannot open " << filename << std::endl;
//         exit(1);
//     }

//     std::string line;

//     while (std::getline(file, line)) {
//         line = trim(line);

//         if (line.empty() || line[0] == '#')
//             continue ;
        
//         if (line == "server {")
//             return parseServer(file);
//     }
//     file.close();
//     return ServerConfig();
// }

LocationConfig* matchLocation(ServerConfig &config, const std::string &path) {
    LocationConfig* best_match = NULL;
    size_t best_length = 0;

    for (size_t i = 0; i < config.locations.size(); i++) {
        std::string loc_path = config.locations[i].path;

        if (path.substr(0, loc_path.size()) == loc_path) {
            if (loc_path.size() > best_length) {
                best_match = &config.locations[i];
                best_length = loc_path.size();
            }
        } 
    }

    return best_match;
}
