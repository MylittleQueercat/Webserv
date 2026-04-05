/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: leticiabi <leticiabi@student.42.fr>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/30 16:51:56 by hguo              #+#    #+#             */
/*   Updated: 2026/04/05 13:01:00 by leticiabi        ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../includes/Webserv.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>

// Removes leading/trailing whitespace and trailing semicolons from a string
std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(" \t;");
    return s.substr(start, end - start + 1);
}

// Converts a size string (e.g. "1m", "10k") to bytes
size_t  parseSize(const std::string &s) {
    size_t  multiplier = 1;
    if (s[s.size() - 1] == 'm')
        multiplier = 1024 * 1024;
    else if (s[s.size() - 1] == 'k')
        multiplier = 1024;
    return atoi(s.c_str()) * multiplier;
}

// Parses a location{} block from the config file and returns a LocationConfig
// Reads line by line until closing brace '}'
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
        else if (key == "return") {
            std::string code, url;
            iss >> code >> url;
            loc.redirect_code = atoi(code.c_str());
            loc.redirect_url  = trim(url);
        }
        else if (key == "root") {
            std::string val;
            iss >> val;
            loc.root = trim(val);
        }
    }
    return loc;
}

// Parses a server{} block from the config file and returns a ServerConfig
// When a location{} block is encountered, delegates to parseLocation()
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
                config.error_pages[atoi(code.c_str())] = trim(path);
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

// Parses the entire config file and returns all server configurations
// Each "server {" block is parsed into a ServerConfig and added to the vector
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
            servers.push_back(parseServer(file));
    }
    file.close();
    return servers;
}

//matchLocation scans location entries(in the file.conf) and returns a pointer to the best matching LocationConfig for a given request path. If nothing matches, it returns NULL.
//longest prefix matcher : it loops through all locations and picks the one whose path is the longest prefix of the request path; Since every path starts with /, the root location always matches as a minimum fallback
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
