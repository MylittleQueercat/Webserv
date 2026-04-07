/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   runServer.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hguo <hguo@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/30 16:54:54 by jili              #+#    #+#             */
/*   Updated: 2026/04/07 17:33:58 by hguo             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../includes/Webserv.hpp"

ClientState::ClientState() : fd(-1), headers_done(false),
                              content_length(0), config(NULL),
                              cgi_pid(-1), cgi_output_fd(-1), cgi_input_fd(-1), 
                              is_cgi(false), cgi_last_activity(0) {}

// Global session storage: session_id -> username
std::map<std::string, std::string> sessions;
// Generate a random 16-character hexadecimal string to use as a unique session ID : a3f9b2c1d4e57f02
	//rand() % 16 → random number between 0 and 15;std::hex → convert it to hexadecimal character
static std::string generateSessionId()
{
    std::ostringstream oss;
    for (int i = 0; i < 16; i++)
        oss << std::hex << (rand() % 16);
    return oss.str();
}

// 1. CGI timeout check : Kills CGI processes that have been running for more than 10 seconds
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

// 2. CGI pipe handler : Returns true if the fd was a CGI pipe and was handled
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
            
            std::cerr << "CGI output size: " << output.size() << std::endl;
            std::cerr << "CGI body size: " << body.size() << std::endl;
        
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

// 3. New client acceptor : Returns true if the fd was a server socket and a new client was accepted
static bool acceptNewClient(size_t i,
                             std::vector<struct pollfd>& fds,
                             std::set<int>& server_fds,
                             std::map<int, ClientState>& clients,
                             std::map<int, ServerConfig*>& fd_to_config)
{
    if (!server_fds.count(fds[i].fd))
        return false;

    int client_fd = accept(fds[i].fd, NULL, NULL);
    if (client_fd < 0)
	{
        std::cerr << "accept() failed" << std::endl;
        return true;
    }
	//O_NONBLOCK tells the OS never to freeze your program waiting for I/O — if nothing is ready, return immediately with EAGAIN so your program can go do something else
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
	//Add the new Client into the poll() listening list 
    struct pollfd client_pfd;
    client_pfd.fd      = client_fd;
    client_pfd.events  = POLLIN;
    client_pfd.revents = 0;
    fds.push_back(client_pfd);
	// set the state.config -> create the lien between client_fd and his server in the ClientState level
    ClientState state;
    state.fd     = client_fd;
    state.config = fd_to_config[fds[i].fd];
    clients[client_fd] = state;

    std::cout << "New client connected fd=" << client_fd
              << " port=" << state.config->port << std::endl;
    return true;
}

// 4. Request completeness check : Returns true when the full HTTP request (headers + body) has been received
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

// 5. Session route handler : Returns the full HTTP response string if the path is a session route (4 session-related routes); returns empty string if not a session route
	//This code uses Cookies for identity during a session (login from submission -> welcome page), not for remembering username across sessions
static std::string handleSessionRoute(const HttpRequest& req)
{
	//login page
    if (req.path == "/login" && req.method == "GET")
	{
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

	//login form submission
    if (req.path == "/login" && req.method == "POST")
	{
        std::string username = "";
        size_t pos = req.body.find("username=");
        if (pos != std::string::npos)
            username = req.body.substr(pos + 9);
        std::string sid = generateSessionId();
        sessions[sid] = username;//in session map
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
	//welcome page : get username from Cookies
    if (req.path == "/welcome") {
        std::string username = "Stranger";
        if (req.headers.count("Cookie"))
		{
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
	// logout
    if (req.path == "/logout")
	{
        if (req.headers.count("Cookie"))
		{
            std::string cookie = req.headers.find("Cookie")->second;
            size_t pos = cookie.find("sid=");
			// delete session from server memory
            if (pos != std::string::npos)
                sessions.erase(cookie.substr(pos + 4));
        }
        std::string body = "<html><body><h1>Logged out</h1>"
                           "<a href='/login'>Login again</a>"
                           "</body></html>";
        std::ostringstream len;
        len << body.size();
		//expire the cookie in the browser
        return "HTTP/1.1 200 OK\r\n"
               "Content-Type: text/html; charset=utf-8\r\n"
               "Set-Cookie: sid=; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n"
               "Content-Length: " + len.str() + "\r\n"
               "\r\n" + body;
    }
	// Not a session route
    return "";
}

//6. Client data handler
static void handleClientData(size_t& i,
                              std::vector<struct pollfd>& fds,
                              std::map<int, ClientState>& clients)
{
	//1. receive data from a connected client
    char buf[4096];
    int  bytes = recv(fds[i].fd, buf, sizeof(buf), 0);
		// simulation of TCP chunks : in real TCP, a single HTTP request might arrive in multiple recv() calls (multuple poll() iterations)
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

    // If CGI is running, pipe new data directly
    if (clients[fds[i].fd].is_cgi && clients[fds[i].fd].cgi_input_fd != -1) {
        size_t header_end = rbuf.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            std::string new_body = rbuf.substr(header_end + 4);
            if (!new_body.empty()) {
                clients[fds[i].fd].cgi_body_buffer += new_body;
                rbuf = rbuf.substr(0, header_end + 4);  // keep only headers
                clients[fds[i].fd].cgi_last_activity = time(NULL);
            }
            bool last_chunk = clients[fds[i].fd].cgi_body_buffer.find("0\r\n\r\n") != std::string::npos;
            if (last_chunk) {
                std::string decoded = unchunk(clients[fds[i].fd].cgi_body_buffer);
                clients[fds[i].fd].cgi_body_buffer.clear();
                clients[fds[i].fd].send_buffer = decoded;
                std::string& sbuf = clients[fds[i].fd].send_buffer;
                int written = write(clients[fds[i].fd].cgi_input_fd, sbuf.c_str(), sbuf.size());
                if (written > 0)
                    sbuf = sbuf.substr(written);
                if (sbuf.empty()) {
                    close(clients[fds[i].fd].cgi_input_fd);
                    clients[fds[i].fd].cgi_input_fd = -1;
                }
            }
        }
        return;  // Don't process as new request
    }
    
    // std::cerr << "recv bytes=" << bytes << " rbuf size=" << rbuf.size() << " is_cgi=" << clients[fds[i].fd].is_cgi << std::endl;
	//2. First body size check (in header) and ask the permission
		// “413 Content Too Large”
    if (rbuf.find("\r\n\r\n") != std::string::npos)
	{
        //printf("First check");
        // std::cerr << "=== REQUEST HEADERS ===" << std::endl;
        // std::cerr << rbuf.substr(0, rbuf.find("\r\n\r\n")) << std::endl;
        // std::cerr << "=======================" << std::endl;
        
        size_t cl_pos = rbuf.find("Content-Length: ");
        if (cl_pos != std::string::npos) 
        {
            printf("————————————————————————First check——————————————");
            size_t cl_end         = rbuf.find("\r\n", cl_pos);
            size_t content_length = atoi(rbuf.substr(cl_pos + 16, cl_end - cl_pos - 16).c_str());
            if (content_length > clients[fds[i].fd].config->max_body)
            {
                std::string resp = buildErrorResponse(413, *clients[fds[i].fd].config);
                send(fds[i].fd, resp.c_str(), resp.size(), 0);
                close(fds[i].fd);
                clients.erase(fds[i].fd);
                fds.erase(fds.begin() + i);
                i--;
                // printf("First check");
                return;
            }
        }
    }
		//"Expect: 100-continue" ：the client asking for permission before sending a large body - the server either say "100 Continue" (go ahead) or rejects early with 413; It is purely a bandwidth-saving negotiation before committing to sending a large body
    if (rbuf.find("Expect: 100-continue") != std::string::npos)
	{
        std::string cont = "HTTP/1.1 100 Continue\r\n\r\n";
        send(fds[i].fd, cont.c_str(), cont.size(), 0);
    }

    // Early CGI launch: start CGI as soon as headers are complete
    if (rbuf.find("\r\n\r\n") != std::string::npos && !clients[fds[i].fd].is_cgi) {
        HttpRequest req = parseRequest(rbuf);
        
        if (req.method == "POST") 
        {
            LocationConfig* loc = matchLocation(*clients[fds[i].fd].config, req.path);
            LocationConfig* loc_with_slash = matchLocation(*clients[fds[i].fd].config, req.path + "/");
            if (loc_with_slash)
                loc = loc_with_slash;
            
            if (loc && !loc->cgi_ext.empty() && req.path.find(loc->cgi_ext) != std::string::npos) {
                std::cerr << "Starting CGI for path: " << req.path << std::endl;
                startCGI(req, *loc, clients[fds[i].fd]);
                std::cerr << "CGI started, input_fd=" << clients[fds[i].fd].cgi_input_fd 
                << " output_fd=" << clients[fds[i].fd].cgi_output_fd << std::endl;
                int input_flags = fcntl(clients[fds[i].fd].cgi_input_fd, F_GETFL, 0);
                fcntl(clients[fds[i].fd].cgi_input_fd, F_SETFL, input_flags | O_NONBLOCK);
                int cgi_flags = fcntl(clients[fds[i].fd].cgi_output_fd, F_GETFL, 0);
                
                fcntl(clients[fds[i].fd].cgi_output_fd, F_SETFL, cgi_flags | O_NONBLOCK);
                
                struct pollfd cgi_pfd;
                cgi_pfd.fd      = clients[fds[i].fd].cgi_output_fd;
                cgi_pfd.events  = POLLIN;
                cgi_pfd.revents = 0;
                fds.push_back(cgi_pfd);

                struct pollfd cgi_input_pfd;
                cgi_input_pfd.fd      = clients[fds[i].fd].cgi_input_fd;
                cgi_input_pfd.events  = POLLOUT;
                cgi_input_pfd.revents = 0;
                fds.push_back(cgi_input_pfd);
                
                clients[fds[i].fd].cgi_last_activity = time(NULL);
            }
        }
    }
    
	//3. parser complete HTTP request and second body size
    if (!isRequestComplete(rbuf)) {
        if (clients[fds[i].fd].is_cgi && clients[fds[i].fd].cgi_input_fd != -1) {
            size_t header_end = rbuf.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                // Move new body data into cgi_body_buffer
                std::string new_body = rbuf.substr(header_end + 4);
                if (!new_body.empty()) {
                    clients[fds[i].fd].cgi_body_buffer += new_body;
                    rbuf = rbuf.substr(0, header_end + 4);  // keep only headers
                    clients[fds[i].fd].cgi_last_activity = time(NULL); 
                    std::cerr << "cgi_body_buffer size: " << clients[fds[i].fd].cgi_body_buffer.size() << std::endl;
                }

                    // ✅ 加在这里
                    if (clients[fds[i].fd].cgi_body_buffer.size() > 100000000) {
                        std::string& cbuf = clients[fds[i].fd].cgi_body_buffer;
                        std::cerr << "last 10 bytes: ";
                        size_t start = cbuf.size() > 10 ? cbuf.size() - 10 : 0;
                        for (size_t k = start; k < cbuf.size(); k++)
                            std::cerr << std::hex << (int)(unsigned char)cbuf[k] << " ";
                        std::cerr << std::endl;
                        // Also print rbuf
                        std::cerr << "rbuf last bytes: ";
                        for (size_t k = 0; k < rbuf.size(); k++)
                            std::cerr << std::hex << (int)(unsigned char)rbuf[k] << " ";
                        std::cerr << std::endl;
                    }
                
                // Check if all chunks received
                bool last_chunk = (clients[fds[i].fd].cgi_body_buffer.find("0\r\n\r\n") != std::string::npos)
                    || (rbuf.find("0\r\n\r\n") != std::string::npos);

                std::cerr << "rbuf size: " << rbuf.size() << std::endl;
                std::cerr << "rbuf find 0rn: " << rbuf.find("0\r\n\r\n") << std::endl;
                
                std::cerr << "last_chunk: " << last_chunk << std::endl;
                
                if (last_chunk) {
                    // All data received, unchunk and store in send_buffer
                    std::string decoded = unchunk(clients[fds[i].fd].cgi_body_buffer);
                    clients[fds[i].fd].cgi_body_buffer.clear();
                    clients[fds[i].fd].send_buffer = decoded;  // store for POLLOUT writing
                    
                    // Try to write as much as possible now
                    std::string& sbuf = clients[fds[i].fd].send_buffer;
                    int written = write(clients[fds[i].fd].cgi_input_fd, sbuf.c_str(), sbuf.size());
                    if (written > 0)
                        sbuf = sbuf.substr(written);
                    
                    // If all written, close pipe
                    if (sbuf.empty()) {
                        close(clients[fds[i].fd].cgi_input_fd);
                        clients[fds[i].fd].cgi_input_fd = -1;
                    }
                    // If not all written, POLLOUT will handle the rest (already registered)
                }
            }
        }
        return;
    }

    if (clients[fds[i].fd].is_cgi && clients[fds[i].fd].cgi_input_fd != -1) {
        close(clients[fds[i].fd].cgi_input_fd);
        clients[fds[i].fd].cgi_input_fd = -1;
        clients[fds[i].fd].recv_buffer.clear();
        return;  // CGI already handled, nothing more to do
    }

    HttpRequest req = parseRequest(rbuf);

		//second body size : in body
    if (req.body.size() > clients[fds[i].fd].config->max_body) {
        std::string resp = buildErrorResponse(413, *clients[fds[i].fd].config);
        send(fds[i].fd, resp.c_str(), resp.size(), 0);
        close(fds[i].fd);
        clients.erase(fds[i].fd);
        fds.erase(fds.begin() + i);
        i--;
        return;
    }

    //4. Session routes (/login, /welcome, /logout)
    std::string session_resp = handleSessionRoute(req);
    if (!session_resp.empty())
	{
        send(fds[i].fd, session_resp.c_str(), session_resp.size(), 0);
        clients[fds[i].fd].recv_buffer.clear();
        return;
    }

    //5. Match location from config
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
	// This is the information to DEBUG?
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
    
    //TO BE DISCUSSED : 6. HTTP redirect (301/302) : We should add the 302 part or not?
    if (loc->redirect_code != 0 && !loc->redirect_url.empty())
	{
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

    //7. Method not allowed check
    bool method_allowed = false;
    for (size_t j = 0; j < loc->methods.size(); j++)
	{
        if (loc->methods[j] == req.method)
            method_allowed = true;
    }
    if (!method_allowed)
	{
        std::string resp = buildErrorResponse(405, *clients[fds[i].fd].config);
        send(fds[i].fd, resp.c_str(), resp.size(), 0);
        clients[fds[i].fd].recv_buffer.clear();
        return;
    }

    //8. CGI request
    if (!loc->cgi_ext.empty() && req.path.find(loc->cgi_ext) != std::string::npos) {
        std::cerr << "CGI check: method=" << req.method << " path=" << req.path << std::endl;
        startCGI(req, *loc, clients[fds[i].fd]);
        std::cerr << "IMMEDIATELY after startCGI: cgi_input_fd=" 
          << clients[fds[i].fd].cgi_input_fd << std::endl;
        std::cerr << "After startCGI, cgi_input_fd=" << clients[fds[i].fd].cgi_input_fd << std::endl;
        
        // ✅ GET 请求没有 body，立刻关闭写端
        if (req.method == "GET") {
            std::cerr << "GET request, closing cgi_input_fd" << std::endl;
            close(clients[fds[i].fd].cgi_input_fd);
            clients[fds[i].fd].cgi_input_fd = -1;
        }
        std::cerr << "Adding cgi_output_fd to poll" << std::endl;
        
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

    //9. Normal GET / POST / DELETE request
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

        for (size_t i = 0; i < fds.size(); i++)
        {
           // ✅ 处理 CGI input pipe 可写事件
            if (fds[i].revents & POLLOUT) {
                for (std::map<int, ClientState>::iterator it = clients.begin();
                    it != clients.end(); ++it) {
                    if (it->second.cgi_input_fd == fds[i].fd) {
                        std::string& sbuf = it->second.send_buffer;
                        if (!sbuf.empty()) {
                            int written = write(fds[i].fd, sbuf.c_str(), sbuf.size());
                            if (written > 0)
                                sbuf = sbuf.substr(written);
                        }
                        // if (sbuf.empty()) {
                        //     close(fds[i].fd);
                        //     fds.erase(fds.begin() + i);
                        //     i--;
                        //     it->second.cgi_input_fd = -1;
                        // }
                        break;
                    }
                }
                continue;
            }
           
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
