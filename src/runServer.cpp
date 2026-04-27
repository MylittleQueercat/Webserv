/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   runServer.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hguo <hguo@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/30 16:54:54 by jili              #+#    #+#             */
/*   Updated: 2026/04/27 17:46:19 by hguo             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../includes/Webserv.hpp"

ClientState::ClientState() : fd(-1),
                              send_offset(0),
                              headers_done(false),
                              content_length(0),
                              config(NULL),
                              cgi_pid(-1),
                              cgi_output_fd(-1),
                              cgi_input_fd(-1),
                              is_cgi(false),
                              cgi_body_mode(false),
                              cgi_body_buffer(""),
                              cgi_stdin_buffer(""),
                              cgi_stdin_sent(0),
                              cgi_output(""),
                              cgi_last_activity(0) {}

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

static std::string toLowerCopy(const std::string& s)
{
    std::string out = s;
    for (size_t i = 0; i < out.size(); i++)
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    return out;
}

static bool hasChunkedTransferEncoding(const std::string& header_part)
{
    std::string lower = toLowerCopy(header_part);
    return lower.find("transfer-encoding: chunked") != std::string::npos;
}

static bool methodAllowed(LocationConfig* loc, const std::string& method)
{
    if (!loc)
        return false;
    for (size_t i = 0; i < loc->methods.size(); i++)
    {
        if (loc->methods[i] == method)
            return true;
    }
    return false;
}

static bool endsWith(const std::string& s, const std::string& suffix)
{
    if (s.size() < suffix.size())
        return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static bool isChunkedBodyComplete(const std::string& body)
{
    return endsWith(body, "\r\n0\r\n\r\n") || endsWith(body, "0\r\n\r\n");
}

static int hexValue(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static bool parseChunkSize(const std::string& line, size_t& size)
{
    size = 0;

    for (size_t i = 0; i < line.size(); i++)
    {
        if (line[i] == ';')
            break;

        int value = hexValue(line[i]);
        if (value < 0)
            return false;

        size = size * 16 + static_cast<size_t>(value);
    }
    return true;
}

static bool unchunkBody(const std::string& raw, std::string& decoded)
{
    size_t pos = 0;
    decoded.clear();

    while (pos < raw.size())
    {
        size_t line_end = raw.find("\r\n", pos);
        if (line_end == std::string::npos)
            return false;

        std::string size_line = raw.substr(pos, line_end - pos);
        size_t chunk_size = 0;

        if (!parseChunkSize(size_line, chunk_size))
            return false;

        pos = line_end + 2;

        if (chunk_size == 0)
            return true;

        if (pos + chunk_size + 2 > raw.size())
            return false;

        decoded.append(raw, pos, chunk_size);
        pos += chunk_size;

        if (raw.substr(pos, 2) != "\r\n")
            return false;

        pos += 2;
    }

    return false;
}

static bool hasExpect100Continue(const std::string& header_part)
{
    std::string lower = toLowerCopy(header_part);
    return lower.find("expect: 100-continue") != std::string::npos;
}

// 1. CGI timeout check : Kills CGI processes that have been running for more than 10 seconds
static void checkCGITimeouts(std::map<int, ClientState>& clients,
                               std::vector<struct pollfd>& fds) {
    for (std::map<int, ClientState>::iterator it = clients.begin();
         it != clients.end(); ++it) {
        ClientState& c = it->second;
        if (!c.is_cgi) continue;
        if (c.cgi_last_activity == 0) continue;

        if (difftime(time(NULL), c.cgi_last_activity) > 60) {
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

static void enablePollOut(int fd, std::vector<struct pollfd>& fds)
{
    for (size_t j = 0; j < fds.size(); j++)
    {
        if (fds[j].fd == fd)
        {
            fds[j].events |= POLLOUT;
            return;
        }
    }
}

static void queueClientResponse(ClientState& client,
                                std::vector<struct pollfd>& fds,
                                std::string& response)
{
    client.send_buffer.swap(response);
    client.send_offset = 0;
    enablePollOut(client.fd, fds);
}

static std::string normalizeCgiHeaders(const std::string& raw_headers)
{
    std::string result;
    size_t pos = 0;

    while (pos < raw_headers.size())
    {
        size_t line_end = raw_headers.find('\n', pos);
        std::string line;

        if (line_end == std::string::npos)
        {
            line = raw_headers.substr(pos);
            pos = raw_headers.size();
        }
        else
        {
            line = raw_headers.substr(pos, line_end - pos);
            pos = line_end + 1;
        }

        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        if (!line.empty())
            result += line + "\r\n";
    }

    return result;
}

static bool handleClientWrite(size_t& i,
                              std::vector<struct pollfd>& fds,
                              std::map<int, ClientState>& clients)
{
    std::map<int, ClientState>::iterator it = clients.find(fds[i].fd);
    if (it == clients.end())
        return false;

    ClientState& client = it->second;

    if (client.send_buffer.empty())
        return false;

    size_t remaining = client.send_buffer.size() - client.send_offset;
    size_t chunk = remaining > 65536 ? 65536 : remaining;

    ssize_t sent = send(client.fd,
                        client.send_buffer.data() + client.send_offset,
                        chunk,
                        0);

    if (sent > 0)
    {
        client.send_offset += static_cast<size_t>(sent);
    }
    else
    {
        std::cerr << "[HTTP response write failed]" << std::endl;
        close(client.fd);
        clients.erase(client.fd);
        fds.erase(fds.begin() + i);
        i--;
        return true;
    }

    if (client.send_offset >= client.send_buffer.size())
    {
        bool close_after_send = false;

        size_t header_end = client.send_buffer.find("\r\n\r\n");
        std::string headers;
        if (header_end != std::string::npos)
            headers = client.send_buffer.substr(0, header_end);
        else
            headers = client.send_buffer;

        headers = toLowerCopy(headers);
        if (headers.find("connection: close") != std::string::npos)
            close_after_send = true;

        std::string().swap(client.send_buffer);
        client.send_offset = 0;

        if (close_after_send)
        {
            close(client.fd);
            clients.erase(client.fd);
            fds.erase(fds.begin() + i);
            i--;
            return true;
        }

        fds[i].events &= ~POLLOUT;
        fds[i].events |= POLLIN;
        return true;
    }

    return true;
}

static bool handleCGIInputPipe(size_t& i,
                               std::vector<struct pollfd>& fds,
                               std::map<int, ClientState>& clients)
{
    ClientState* cgi_client = NULL;

    for (std::map<int, ClientState>::iterator it = clients.begin();
         it != clients.end(); ++it)
    {
        if (it->second.cgi_input_fd == fds[i].fd)
        {
            cgi_client = &it->second;
            break;
        }
    }

    if (!cgi_client)
        return false;

    size_t remaining = cgi_client->cgi_stdin_buffer.size() - cgi_client->cgi_stdin_sent;
    size_t chunk = remaining > 65536 ? 65536 : remaining;

    if (chunk > 0)
    {
        ssize_t written = write(fds[i].fd,
                                cgi_client->cgi_stdin_buffer.data() + cgi_client->cgi_stdin_sent,
                                chunk);

        if (written > 0)
        {
            cgi_client->cgi_stdin_sent += static_cast<size_t>(written);
            cgi_client->cgi_last_activity = time(NULL);
        }
        else
        {
            std::cerr << "[CGI stdin write failed or would block]" << std::endl;
        }
    }

    if (cgi_client->cgi_stdin_sent >= cgi_client->cgi_stdin_buffer.size())
    {
        close(fds[i].fd);
        fds.erase(fds.begin() + i);
        i--;

        cgi_client->cgi_input_fd = -1;
        std::string().swap(cgi_client->cgi_stdin_buffer);
        cgi_client->cgi_stdin_sent = 0;
    }

    return true;
}

// 2. CGI pipe handler : Returns true if the fd was a CGI pipe and was handled
static bool handleCGIPipe(size_t& i,
                          std::vector<struct pollfd>& fds,
                          std::map<int, ClientState>& clients)
{
    ClientState* cgi_client = NULL;

    for (std::map<int, ClientState>::iterator it = clients.begin();
         it != clients.end(); ++it)
    {
        if (it->second.cgi_output_fd == fds[i].fd)
        {
            cgi_client = &it->second;
            break;
        }
    }

    if (!cgi_client)
        return false;

    char buf[4096];
    int bytes = read(fds[i].fd, buf, sizeof(buf));

    if (bytes > 0)
    {
        cgi_client->cgi_output += std::string(buf, bytes);
        cgi_client->cgi_last_activity = time(NULL);
        return true;
    }

    if (bytes == 0 || (fds[i].revents & (POLLHUP | POLLERR)))
    {
        close(fds[i].fd);
        fds.erase(fds.begin() + i);
        i--;

        waitpid(cgi_client->cgi_pid, NULL, WNOHANG);
        cgi_client->is_cgi = false;
        cgi_client->cgi_output_fd = -1;

        std::string& output = cgi_client->cgi_output;
        std::string response;

        size_t header_end = output.find("\r\n\r\n");
        size_t sep_len = 4;

        if (header_end == std::string::npos)
        {
            header_end = output.find("\n\n");
            sep_len = 2;
        }

        if (header_end != std::string::npos)
        {
            std::string raw_cgi_headers = output.substr(0, header_end);
            std::string cgi_headers = normalizeCgiHeaders(raw_cgi_headers);
            size_t body_start = header_end + sep_len;
            size_t body_size = output.size() - body_start;

            std::ostringstream oss;
            oss << body_size;

            response.reserve(128 + cgi_headers.size() + body_size);
            response = "HTTP/1.1 200 OK\r\n";
            response += "Connection: close\r\n";
            response += cgi_headers;
            response += "Content-Length: " + oss.str() + "\r\n\r\n";
            response.append(output, body_start, body_size);
        }
        else
        {
            std::cerr << "[CGI no header separator found, treating all as body] size="
                    << output.size()
                    << std::endl;

            std::ostringstream oss;
            oss << output.size();

            response  = "HTTP/1.1 200 OK\r\n";
            response += "Connection: close\r\n";
            response += "Content-Length: " + oss.str() + "\r\n\r\n";
            response += output;
        }

        queueClientResponse(*cgi_client, fds, response);
        std::string().swap(cgi_client->cgi_output);
        return true;
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
        return true;
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
                              std::map<int, ClientState>& clients,std::vector<ServerConfig>& configs )
{
	//1. receive data from a connected client
    char buf[4096];
    int  bytes = recv(fds[i].fd, buf, sizeof(buf), 0);
		// simulation of TCP chunks : in real TCP, a single HTTP request might arrive in multiple recv() calls (multuple poll() iterations)
    if (bytes <= 0) {
        close(fds[i].fd);
        clients.erase(fds[i].fd);
        fds.erase(fds.begin() + i);
        i--;
        return;
    }

    ClientState& client = clients[fds[i].fd];

    if (client.cgi_body_mode)
    {
        client.cgi_body_buffer.append(buf, bytes);
        client.cgi_last_activity = time(NULL);

        if (isChunkedBodyComplete(client.cgi_body_buffer))
        {
            if (!unchunkBody(client.cgi_body_buffer, client.cgi_stdin_buffer))
            {
                std::cerr << "[CGI body error] invalid chunked body" << std::endl;
                close(client.cgi_input_fd);
                client.cgi_input_fd = -1;
                client.cgi_body_mode = false;
                return;
            }

            client.cgi_stdin_sent = 0;
            client.cgi_body_mode = false;
            std::string().swap(client.cgi_body_buffer);

            struct pollfd input_pfd;
            input_pfd.fd = client.cgi_input_fd;
            input_pfd.events = POLLOUT;
            input_pfd.revents = 0;
            fds.push_back(input_pfd);
        }

        return;
    }

    client.recv_buffer += std::string(buf, bytes);
    std::string& rbuf = client.recv_buffer;
	//2. First body size check (in header) and ask the permission
		// “413 Content Too Large”
    if (rbuf.find("\r\n\r\n") != std::string::npos)
	{
        size_t cl_pos = rbuf.find("Content-Length: ");
        if (cl_pos != std::string::npos) {
            size_t cl_end         = rbuf.find("\r\n", cl_pos);
            size_t content_length = atoi(rbuf.substr(cl_pos + 16, cl_end - cl_pos - 16).c_str());
            if (content_length > clients[fds[i].fd].config->max_body) {
                std::string resp = buildErrorResponse(413, *clients[fds[i].fd].config);
                queueClientResponse(clients[fds[i].fd], fds, resp);
                clients[fds[i].fd].recv_buffer.clear();
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

    size_t header_end_for_cgi = rbuf.find("\r\n\r\n");

    if (header_end_for_cgi != std::string::npos)
    {
        std::string header_part = rbuf.substr(0, header_end_for_cgi + 4);

        if (hasChunkedTransferEncoding(header_part))
        {
            HttpRequest head_req = parseRequest(header_part);

            LocationConfig* cgi_loc = matchLocation(*client.config, head_req.path);
            LocationConfig* cgi_loc_with_slash = matchLocation(*client.config, head_req.path + "/");

            if (cgi_loc_with_slash)
                cgi_loc = cgi_loc_with_slash;

            if (cgi_loc &&
                methodAllowed(cgi_loc, head_req.method) &&
                !cgi_loc->cgi_ext.empty() &&
                head_req.path.find(cgi_loc->cgi_ext) != std::string::npos)
            {
                if (hasExpect100Continue(header_part))
                {
                    std::string cont = "HTTP/1.1 100 Continue\r\n\r\n";
                    ssize_t sent = send(client.fd, cont.c_str(), cont.size(), 0);
                    std::cerr << "[100 Continue sent] fd=" << client.fd
                            << " sent=" << sent
                            << std::endl;
                }
                
                startCGI(head_req, *cgi_loc, client, true);

                int out_flags = fcntl(client.cgi_output_fd, F_GETFL, 0);
                fcntl(client.cgi_output_fd, F_SETFL, out_flags | O_NONBLOCK);

                int in_flags = fcntl(client.cgi_input_fd, F_GETFL, 0);
                fcntl(client.cgi_input_fd, F_SETFL, in_flags | O_NONBLOCK);

                struct pollfd cgi_pfd;
                cgi_pfd.fd      = client.cgi_output_fd;
                cgi_pfd.events  = POLLIN;
                cgi_pfd.revents = 0;
                fds.push_back(cgi_pfd);

                client.cgi_last_activity = time(NULL);
                client.cgi_body_mode = true;

                std::string already_received_body = rbuf.substr(header_end_for_cgi + 4);
                if (!already_received_body.empty())
                {
                    client.cgi_body_buffer.append(already_received_body);
                }

                client.recv_buffer.clear();
                return;
            }
        }
    }

    //3. parser complete HTTP request and second body size
    if (!isRequestComplete(rbuf))
        return;

    HttpRequest req = parseRequest(rbuf);
    if (req.headers.count("Host"))
    {
        std::string host = req.headers["Host"];
        size_t colon = host.find(':');
        if (colon != std::string::npos)
            host = host.substr(0, colon);
        for (size_t s = 0; s < configs.size(); s++)
        {
            if (configs[s].server_name == host && configs[s].port == clients[fds[i].fd].config->port)
            {
                clients[fds[i].fd].config = &configs[s];
                break;
            }
        }
    }

		//second body size : in body
    if (req.body.size() > clients[fds[i].fd].config->max_body) {
        std::string resp = buildErrorResponse(413, *clients[fds[i].fd].config);
        queueClientResponse(clients[fds[i].fd], fds, resp);
        clients[fds[i].fd].recv_buffer.clear();
        return;
    }

    //4. Session routes (/login, /welcome, /logout)
    std::string session_resp = handleSessionRoute(req);
    if (!session_resp.empty())
	{
        queueClientResponse(clients[fds[i].fd], fds, session_resp);
        clients[fds[i].fd].recv_buffer.clear();
        return;
    }

    //5. Match location from config
    LocationConfig* loc = matchLocation(*clients[fds[i].fd].config, req.path);
    LocationConfig* loc_with_slash = matchLocation(*clients[fds[i].fd].config, req.path + "/");

    if (loc_with_slash)
        loc = loc_with_slash;  // prefer more specific match

    if (!loc) {
        std::string resp = buildErrorResponse(404, *clients[fds[i].fd].config);
        queueClientResponse(clients[fds[i].fd], fds, resp);
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
        queueClientResponse(clients[fds[i].fd], fds, resp);
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
        queueClientResponse(clients[fds[i].fd], fds, resp);
        clients[fds[i].fd].recv_buffer.clear();
        return;
    }

    //8. CGI request
    if (req.method == "POST" &&
        !loc->cgi_ext.empty() &&
        req.path.find(loc->cgi_ext) != std::string::npos)
    {
        ClientState& cgi_client = clients[fds[i].fd];

        startCGI(req, *loc, cgi_client, false);

        int out_flags = fcntl(cgi_client.cgi_output_fd, F_GETFL, 0);
        fcntl(cgi_client.cgi_output_fd, F_SETFL, out_flags | O_NONBLOCK);

        int in_flags = fcntl(cgi_client.cgi_input_fd, F_GETFL, 0);
        fcntl(cgi_client.cgi_input_fd, F_SETFL, in_flags | O_NONBLOCK);

        struct pollfd output_pfd;
        output_pfd.fd      = cgi_client.cgi_output_fd;
        output_pfd.events  = POLLIN;
        output_pfd.revents = 0;
        fds.push_back(output_pfd);

        cgi_client.cgi_stdin_buffer = req.body;
        cgi_client.cgi_stdin_sent = 0;

        struct pollfd input_pfd;
        input_pfd.fd      = cgi_client.cgi_input_fd;
        input_pfd.events  = POLLOUT;
        input_pfd.revents = 0;
        fds.push_back(input_pfd);

        cgi_client.cgi_last_activity = time(NULL);
        cgi_client.recv_buffer.clear();
        return;
    }

    // Check body size limit before handling normal request
    size_t max_body = clients[fds[i].fd].config->max_body;

    if (loc && loc->has_max_body)
        max_body = loc->max_body;

    if (req.body.size() > max_body)
    {
        std::string resp = buildErrorResponse(413, *clients[fds[i].fd].config);
        queueClientResponse(clients[fds[i].fd], fds, resp);
        clients[fds[i].fd].recv_buffer.clear();
        return;
    }
    
    //9. Normal GET / POST / DELETE request
    std::string resp = handleRequest(req, *clients[fds[i].fd].config, *loc);
    queueClientResponse(clients[fds[i].fd], fds, resp);
    clients[fds[i].fd].recv_buffer.clear();
    return;
}

// ── Main server loop ───────────────────────────────────────────────────────
void runServer(std::vector<ServerConfig>& configs) {
    std::vector<struct pollfd> fds;//master watch list 
    std::set<int>              server_fds;
    std::map<int, ClientState> clients;//client fd-> ClientState
    std::map<int, ServerConfig*> fd_to_config;//server fd -> ServerConfig*

    for (size_t s = 0; s < configs.size(); s++)
    // std::cerr << "Config " << s << ": port=" << configs[s].port 
    //           << " server_name=" << configs[s].server_name 
    //           << " root=" << configs[s].root << std::endl;
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
            if (!(fds[i].revents & (POLLIN | POLLOUT | POLLHUP | POLLERR | POLLNVAL)))
                continue;

            if ((fds[i].revents & POLLOUT) && handleCGIInputPipe(i, fds, clients))
                continue;

            if ((fds[i].revents & POLLOUT) && handleClientWrite(i, fds, clients))
                continue;
                
            if ((fds[i].revents & (POLLIN | POLLHUP | POLLERR)) &&
                handleCGIPipe(i, fds, clients))
                continue;

            if ((fds[i].revents & POLLIN) &&
                acceptNewClient(i, fds, server_fds, clients, fd_to_config))
                continue;

            if (fds[i].revents & POLLIN)
                handleClientData(i, fds, clients, configs);
        }
    }
}
