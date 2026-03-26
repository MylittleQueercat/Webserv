#ifndef CONFIGPARSER_HPP
# define CONFIGPARSER_HPP

# include <string>
# include <map>
# include <vector>

struct RawRequest {
    int         client_fd;
    std::string raw_data;
};

struct RawResponse {
    int         client_fd;
    std::string raw_data;
    bool        close_after;
};

struct LocationConfig {
    std::string              path;         // URL 
    std::vector<std::string> methods;      // GET / POST
    std::string              root;         // "/var/www"
    std::string              index;        // "index.html" Le fichier à afficher par défaut quand on accède à un dossier
    bool                     autoindex;    // true/false affiche la liste des fichiers ou pas?
    std::string              upload_store; // "/tmp/uploads" Le dossier où les fichiers envoyés par le client seront sauvegardés
    std::string              cgi_ext;
    int                      redirect_code;
    std::string              redirect_url;

    LocationConfig() : autoindex(false), redirect_code(0) {} 
};

struct ServerConfig {
    int                          port;        // 8080
    int                          server_fd;
    std::string                  root;        // "/var/www/html"
    // std::string                  error_page;  // "/404.html"
    std::map<int, std::string> error_pages;
    size_t                       max_body;    // 1048576 (1m)
    std::vector<LocationConfig>  locations;

    //构造函数，设置默认值
    ServerConfig() : port(80), max_body(1048576) {}
};

std::vector<ServerConfig>    parseConfigs(const std::string &filename);
LocationConfig* matchLocation(ServerConfig &config, const std::string &path);

#endif