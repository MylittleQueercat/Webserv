/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   runServer.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hguo <hguo@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/30 16:54:54 by jili              #+#    #+#             */
/*   Updated: 2026/04/02 14:10:46 by hguo             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../includes/Webserv.hpp"

ClientState::ClientState() : fd(-1), headers_done(false),
                              content_length(0), config(NULL),
                              cgi_pid(-1), cgi_output_fd(-1), is_cgi(false),
                              cgi_last_activity(0) {}

// Global session storage: session_id -> username
std::map<std::string, std::string> sessions;

static std::string generateSessionId() {
    std::ostringstream oss;
    for (int i = 0; i < 16; i++)
        oss << std::hex << (rand() % 16);
    return oss.str();
}

// ── 1. CGI timeout check ───────────────────────────────────────────────────
// Kills CGI processes that have been running for more than 10 seconds
static void checkCGITimeouts(std::map<int, ClientState>& clients,
                               std::vector<struct pollfd>& fds) {
    for (std::map<int, ClientState>::iterator it = clients.begin();
         it != clients.end(); ++it) {
        ClientState& c = it->second;
        if (!c.is_cgi) continue;
        if (c.cgi_last_activity == 0) continue;

        if (difftime(time(NULL), c.cgi_last_activity) > 10) {
            std::cerr << "CGI timeout: pid=" << c.cgi_pid << std::endl;

            kill(c.cgi_pid, SIGKILL);
            waitpid(c.cgi_pid, NULL, WNOHANG);

            for (size_t j = 0; j < fds.size(); j++) {
                if (fds[j].fd == c.cgi_output_fd) {
                    close(fds[j].fd);
                    fds.erase(fds.begin() + j);
                    break;
                }
            }
            c.cgi_output_fd = -1;
            c.is_cgi        = false;

            std::string resp = "HTTP/1.1 504 Gateway Timeout\r\n"
                               "Content-Length: 0\r\n\r\n";
            send(c.fd, resp.c_str(), resp.size(), 0);
        }
    }
}

// ── 2. CGI pipe handler ────────────────────────────────────────────────────
// Returns true if the fd was a CGI pipe and was handled
static bool handleCGIPipe(size_t& i,
                           std::vector<struct pollfd>& fds,
                           std::map<int, ClientState>& clients) {
    ClientState* cgi_client = NULL;

    for (std::map<int, ClientState>::iterator it = clients.begin();
         it != clients.end(); ++it) {
        if (it->second.cgi_output_fd == fds[i].fd) {
            cgi_client = &it->second;
            break;
        }
    }

    if (!cgi_client)
        return false;

    char buf[4096];
    int  bytes = read(fds[i].fd, buf, sizeof(buf));

    if (bytes > 0) {
        // Accumulate data and update activity timestamp
        cgi_client->cgi_output += std::string(buf, bytes);
        cgi_client->cgi_last_activity = time(NULL);
    }
    else if (bytes == 0) {
        // EOF: CGI process exited, all data received
        close(fds[i].fd);
        fds.erase(fds.begin() + i);
        i--;

        waitpid(cgi_client->cgi_pid, NULL, WNOHANG);
        cgi_client->is_cgi        = false;
        cgi_client->cgi_output_fd = -1;

        std::string& output = cgi_client->cgi_output;
        std::string  response;

        size_t header_end = output.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            std::string cgi_headers = output.substr(0, header_end);
            std::string body        = output.substr(header_end + 4);
            std::ostringstream oss;
            oss << body.size();

            response  = "HTTP/1.1 200 OK\r\n";
            response += "Connection: close\r\n";
            response += cgi_headers + "\r\n";
            response += "Content-Length: " + oss.str() + "\r\n\r\n";
            response += body;
        } else {
            // No headers from CGI, treat all output as body
            std::ostringstream oss;
            oss << output.size();
            response  = "HTTP/1.1 200 OK\r\n";
            response += "Connection: close\r\n";
            response += "Content-Length: " + oss.str() + "\r\n\r\n";
            response += output;
        }

        send(cgi_client->fd, response.c_str(), response.size(), 0);
        cgi_client->cgi_output.clear();
    }
    else {
        // Read error: clean up and send 500
        close(fds[i].fd);
        fds.erase(fds.begin() + i);
        i--;

        kill(cgi_client->cgi_pid, SIGKILL);
        waitpid(cgi_client->cgi_pid, NULL, WNOHANG);
        cgi_client->is_cgi        = false;
        cgi_client->cgi_output_fd = -1;

        std::string err = "HTTP/1.1 500 Internal Server Error\r\n"
                          "Content-Length: 0\r\n\r\n";
        send(cgi_client->fd, err.c_str(), err.size(), 0);
    }
    return true;
}

// ── 3. New client acceptor ─────────────────────────────────────────────────
// Returns true if the fd was a server socket and a new client was accepted
static bool acceptNewClient(size_t i,
                             std::vector<struct pollfd>& fds,
                             std::set<int>& server_fds,
                             std::map<int, ClientState>& clients,
                             std::map<int, ServerConfig*>& fd_to_config) {
    if (!server_fds.count(fds[i].fd))
        return false;
    
    int client_fd = accept(fds[i].fd, NULL, NULL);
    if (client_fd < 0) {
        std::cerr << "accept() failed" << std::endl;
        return true;
    }

    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    struct pollfd client_pfd;
    client_pfd.fd      = client_fd;
    client_pfd.events  = POLLIN;
    client_pfd.revents = 0;
    fds.push_back(client_pfd);

    ClientState state;
    state.fd     = client_fd;
    state.config = fd_to_config[fds[i].fd];
    clients[client_fd] = state;

    std::cout << "New client connected fd=" << client_fd
              << " port=" << state.config->port << std::endl;
    return true;
}

// ── 4. Request completeness check ─────────────────────────────────────────
// Returns true when the full HTTP request (headers + body) has been received
static bool isRequestComplete(const std::string& rbuf) {
    if (rbuf.find("\r\n\r\n") == std::string::npos)
        return false;

    if (rbuf.find("Transfer-Encoding: chunked") != std::string::npos)
        return rbuf.find("0\r\n\r\n") != std::string::npos;

    size_t header_end = rbuf.find("\r\n\r\n");
    size_t cl_pos     = rbuf.find("Content-Length: ");
    if (cl_pos == std::string::npos)
        return true; // No body expected

    size_t cl_end          = rbuf.find("\r\n", cl_pos);
    size_t content_length  = atoi(rbuf.substr(cl_pos + 16, cl_end - cl_pos - 16).c_str());
    size_t body_received   = rbuf.size() - (header_end + 4);
    return body_received >= content_length;
}

// ── 5. Session route handler ───────────────────────────────────────────────
// Returns the full HTTP response string if the path is a session route,
// returns empty string if not a session route
static std::string handleSessionRoute(const HttpRequest& req) {
    if (req.path == "/login" && req.method == "GET") {
        std::string body = "<html><body>"
                           "<form method='POST' action='/login'>"
                           "Username: <input name='username' type='text'/>"
                           "<input type='submit' value='Login'/>"
                           "</form></body></html>";
        std::ostringstream len;
        len << body.size();
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/html; charset=utf-8\r\n"
               "Content-Length: " + len.str() + "\r\n"
               "\r\n" + body;
    }

    if (req.path == "/login" && req.method == "POST") {
        std::string username = "";
        size_t pos = req.body.find("username=");
        if (pos != std::string::npos)
            username = req.body.substr(pos + 9);

        std::string sid = generateSessionId();
        sessions[sid] = username;

        std::string body = "<html><body><h1>Login successful!</h1>"
                           "<a href='/welcome'>Go to welcome page</a>"
                           "</body></html>";
        std::ostringstream len;
        len << body.size();
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/html; charset=utf-8\r\n"
               "Set-Cookie: sid=" + sid + "; Path=/\r\n"
               "Content-Length: " + len.str() + "\r\n"
               "\r\n" + body;
    }

    if (req.path == "/welcome") {
        std::string username = "Stranger";
        if (req.headers.count("Cookie")) {
            std::string cookie = req.headers.find("Cookie")->second;
            size_t pos = cookie.find("sid=");
            if (pos != std::string::npos) {
                std::string sid = cookie.substr(pos + 4);
                if (sessions.count(sid))
                    username = sessions[sid];
            }
        }
        std::string body = "<html><body><h1>Welcome back, " + username + "!</h1>"
                           "<a href='/logout'>Logout</a>"
                           "</body></html>";
        std::ostringstream len;
        len << body.size();
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/html; charset=utf-8\r\n"
               "Content-Length: " + len.str() + "\r\n"
               "\r\n" + body;
    }

    if (req.path == "/logout") {
        if (req.headers.count("Cookie")) {
            std::string cookie = req.headers.find("Cookie")->second;
            size_t pos = cookie.find("sid=");
            if (pos != std::string::npos)
                sessions.erase(cookie.substr(pos + 4));
        }
        std::string body = "<html><body><h1>Logged out</h1>"
                           "<a href='/login'>Login again</a>"
                           "</body></html>";
        std::ostringstream len;
        len << body.size();
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/html; charset=utf-8\r\n"
               "Set-Cookie: sid=; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n"
               "Content-Length: " + len.str() + "\r\n"
               "\r\n" + body;
    }

    return ""; // Not a session route
}

// ── 6. Client data handler ─────────────────────────────────────────────────
// Receives data from a connected client, parses the request, and sends a response
static void handleClientData(size_t& i,
                              std::vector<struct pollfd>& fds,
                              std::map<int, ClientState>& clients) {
    char buf[4096];
    int  bytes = recv(fds[i].fd, buf, sizeof(buf), 0);

    if (bytes <= 0) {
        std::cout << "Client disconnected fd=" << fds[i].fd << std::endl;
        close(fds[i].fd);
        clients.erase(fds[i].fd);
        fds.erase(fds.begin() + i);
        i--;
        return;
    }

    clients[fds[i].fd].recv_buffer += std::string(buf, bytes);
    std::string& rbuf = clients[fds[i].fd].recv_buffer;

    // Early 413 check: reject oversized body before it is fully received
    if (rbuf.find("\r\n\r\n") != std::string::npos) {
        size_t cl_pos = rbuf.find("Content-Length: ");
        if (cl_pos != std::string::npos) {
            size_t cl_end         = rbuf.find("\r\n", cl_pos);
            size_t content_length = atoi(rbuf.substr(cl_pos + 16, cl_end - cl_pos - 16).c_str());
            if (content_length > clients[fds[i].fd].config->max_body) {
                std::string resp = buildErrorResponse(413, *clients[fds[i].fd].config);
                send(fds[i].fd, resp.c_str(), resp.size(), 0);
                close(fds[i].fd);
                clients.erase(fds[i].fd);
                fds.erase(fds.begin() + i);
                i--;
                return;
            }
        }
    }

    if (rbuf.find("Expect: 100-continue") != std::string::npos) {
        std::string cont = "HTTP/1.1 100 Continue\r\n\r\n";
        send(fds[i].fd, cont.c_str(), cont.size(), 0);
    }

    if (!isRequestComplete(rbuf))
        return;

    HttpRequest req = parseRequest(rbuf);

    // Post-parse body size check (safety net)
    if (req.body.size() > clients[fds[i].fd].config->max_body) {
        std::string resp = buildErrorResponse(413, *clients[fds[i].fd].config);
        send(fds[i].fd, resp.c_str(), resp.size(), 0);
        close(fds[i].fd);
        clients.erase(fds[i].fd);
        fds.erase(fds.begin() + i);
        i--;
        return;
    }

    // Session routes (/login, /welcome, /logout)
    std::string session_resp = handleSessionRoute(req);
    if (!session_resp.empty()) {
        send(fds[i].fd, session_resp.c_str(), session_resp.size(), 0);
        clients[fds[i].fd].recv_buffer.clear();
        return;
    }

    // Match location from config
    LocationConfig* loc = matchLocation(*clients[fds[i].fd].config, req.path);

    // if (!loc)
    // {
    //     std::string redirectPath = req.path + "/";
    //     LocationConfig* locWithSlash = matchLocation(*clients[fds[i].fd].config, redirectPath);
    //     if (locWithSlash)
    //     {
    //         std::string resp = "HTTP/1.1 301 Moved Permanently\r\nLocation: " + redirectPath + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    //         send(fds[i].fd, resp.c_str(), resp.size(), 0);
    //         clients[fds[i].fd].recv_buffer.clear();
    //         return;
    //     }
    //     std::string resp = buildErrorResponse(404, *clients[fds[i].fd].config);
    //     send(fds[i].fd, resp.c_str(), resp.size(), 0);
    //     clients[fds[i].fd].recv_buffer.clear();
    //     return;
    // }

    LocationConfig* loc_with_slash = matchLocation(*clients[fds[i].fd].config, req.path + "/");

    std::cerr << "req.path: [" << req.path << "]" << std::endl;
    std::cerr << "loc: [" << (loc ? loc->path : "NULL") << "]" << std::endl;
    std::cerr << "loc_with_slash: [" << (loc_with_slash ? loc_with_slash->path : "NULL") << "]" << std::endl;


    if (loc_with_slash)
        loc = loc_with_slash;  // prefer more specific match

    if (!loc) {
        std::string resp = buildErrorResponse(404, *clients[fds[i].fd].config);
        send(fds[i].fd, resp.c_str(), resp.size(), 0);
        clients[fds[i].fd].recv_buffer.clear();
        return;
    }
    
    // HTTP redirect (301/302)
    if (loc->redirect_code != 0 && !loc->redirect_url.empty()) {
        std::ostringstream oss;
        oss << loc->redirect_code;
        std::string status_text = (loc->redirect_code == 301) ? "Moved Permanently" : "Found";
        std::string resp = "HTTP/1.1 " + oss.str() + " " + status_text + "\r\n"
                           "Location: " + loc->redirect_url + "\r\n"
                           "Content-Length: 0\r\n\r\n";
        send(fds[i].fd, resp.c_str(), resp.size(), 0);
        clients[fds[i].fd].recv_buffer.clear();
        return;
    }

    // Method not allowed check
    bool method_allowed = false;
    for (size_t j = 0; j < loc->methods.size(); j++) {
        if (loc->methods[j] == req.method)
            method_allowed = true;
    }
    if (!method_allowed) {
        std::string resp = buildErrorResponse(405, *clients[fds[i].fd].config);
        send(fds[i].fd, resp.c_str(), resp.size(), 0);
        clients[fds[i].fd].recv_buffer.clear();
        return;
    }

    // CGI request
    if (!loc->cgi_ext.empty() && req.path.find(loc->cgi_ext) != std::string::npos) {
        startCGI(req, *loc, clients[fds[i].fd]);

        int cgi_flags = fcntl(clients[fds[i].fd].cgi_output_fd, F_GETFL, 0);
        fcntl(clients[fds[i].fd].cgi_output_fd, F_SETFL, cgi_flags | O_NONBLOCK);

        struct pollfd cgi_pfd;
        cgi_pfd.fd      = clients[fds[i].fd].cgi_output_fd;
        cgi_pfd.events  = POLLIN;
        cgi_pfd.revents = 0;
        fds.push_back(cgi_pfd);

        clients[fds[i].fd].cgi_last_activity = time(NULL);
        clients[fds[i].fd].recv_buffer.clear();
        return;
    }

    // Normal GET / POST / DELETE request
    std::string resp = handleRequest(req, *clients[fds[i].fd].config, *loc);
    send(fds[i].fd, resp.c_str(), resp.size(), 0);
    clients[fds[i].fd].recv_buffer.clear();
}

// ── Main server loop ───────────────────────────────────────────────────────
void runServer(std::vector<ServerConfig>& configs) {
    std::vector<struct pollfd> fds;//master watch list 
    std::set<int>              server_fds;
    std::map<int, ClientState> clients;//client fd-> ClientState
    std::map<int, ServerConfig*> fd_to_config;//server fd -> ServerConfig*

    // Register all server sockets
    for (size_t i = 0; i < configs.size(); i++) {
        struct pollfd pfd;
        pfd.fd      = configs[i].server_fd;
        pfd.events  = POLLIN;
        pfd.revents = 0;
        fds.push_back(pfd);
        server_fds.insert(configs[i].server_fd);
        fd_to_config[configs[i].server_fd] = &configs[i];
    }

    while (true) {
        // Timeout of 1 second ensures CGI timeout check runs every second
        // poll() monitors all fds simultaneously for a maximum of 1000ms. If one or more events occur before the timeour expires, poll() will return immediatly with "ready" equal to the number of fds that have events at that moment. If no event occurs within 1000ms, poll() returns anyway with "ready = 0" -- this guaranteed wake-up ensures that "checkCGITimeouts(clients, fds)" is called at least once per second, even when the server is completely idle.
        int ready = poll(&fds[0], fds.size(), 1000);
        if (ready < 0) {
            std::cerr << "poll() failed" << std::endl;
            break;
        }

        checkCGITimeouts(clients, fds);

        for (size_t i = 0; i < fds.size(); i++) {
            if (!(fds[i].revents & POLLIN) && !(fds[i].revents & POLLHUP))
                continue;

            if (handleCGIPipe(i, fds, clients))
                continue;

            if (acceptNewClient(i, fds, server_fds, clients, fd_to_config))
                continue;

            handleClientData(i, fds, clients);
        }
    }
}
