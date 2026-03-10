#ifndef WEBSERV_HPP
# define WEBSERV_HPP

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
};

struct ServerConfig {
    int                          port;        // 8080
    std::string                  root;        // "/var/www/html"
    std::string                  error_page;  // "/404.html"
    size_t                       max_body;    // 1048576 (1m)
    std::vector<LocationConfig>  locations;
};

#endif