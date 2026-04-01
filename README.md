*This project has been created as part of the 42 curriculum by hguo, jili.*

# Webserv

## Description

Webserv is an HTTP/1.1 server written in C++98, built from scratch as part of the 42 school curriculum. The goal is to understand how web servers work by implementing one — handling connections, parsing HTTP requests, serving static files, executing CGI scripts, and managing multiple clients simultaneously using non-blocking I/O.

Key features:
- Non-blocking I/O with a single `poll()` event loop
- Multiple server blocks listening on different ports
- HTTP methods: GET, POST, DELETE
- Static file serving with configurable root directory
- File upload support
- CGI execution (Python, PHP, Bash) with timeout handling
- Directory listing (autoindex)
- Custom error pages
- HTTP redirections (301/302)
- Cookie and session management
- Path traversal protection

## Instructions

### Requirements

- C++98 compiler (`c++`)
- Python3 (for CGI)
- PHP (for CGI)
- Bash (for CGI)

### Compilation

```bash
make
```

### Execution

```bash
./webserv [configuration file]
```

Example:

```bash
./webserv src/config.conf
```

### Configuration file

The configuration file follows a syntax inspired by NGINX. Example:

```nginx
server {
    listen 8080;
    root ./www;
    client_max_body_size 1m;
    error_page 404 /errors/404.html;
    error_page 405 /errors/405.html;
    error_page 413 /errors/413.html;
    error_page 500 /errors/500.html;

    location / {
        methods GET POST;
        autoindex on;
        index index.html;
    }

    location /upload {
        methods POST DELETE;
        upload_store ./uploads;
    }

    location /cgi-bin {
        methods GET POST;
        cgi_ext .py;
    }

    location /old {
        return 301 /index.html;
    }
}

server {
    listen 9090;
    root ./www;

    location / {
        methods GET;
        index index.html;
    }
}
```

### Testing

Basic functionality:

```bash
# GET
curl http://localhost:8080/index.html

# POST file upload
curl -X POST http://localhost:8080/upload --data-binary @file.txt

# DELETE
curl -X DELETE http://localhost:8080/upload/uploaded_file

# CGI
curl http://localhost:8080/cgi-bin/script.py
```

Stress test with Siege:

```bash
siege -b -t 30S http://localhost:8080/
```

Cookie and session demo:

Open `http://localhost:8080/login` in a browser, enter a username, and navigate to `/welcome`. The server remembers the session across page refreshes using cookies.

## Resources

### References

- [RFC 7230 - HTTP/1.1 Message Syntax and Routing](https://datatracker.ietf.org/doc/html/rfc7230)
- [RFC 7231 - HTTP/1.1 Semantics and Content](https://datatracker.ietf.org/doc/html/rfc7231)
- [CGI RFC 3875](https://datatracker.ietf.org/doc/html/rfc3875)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [NGINX documentation](https://nginx.org/en/docs/) — used as reference for configuration file syntax and server behavior
- `man poll`, `man socket`, `man fork`, `man execve`

### Useful functions
https://cplusplus.com/reference/sstream/ostringstream/  
https://en.cppreference.com/w/cpp/io/basic_ifstream.html  
https://www.php.net/manual/en/function.realpath.php  
https://man7.org/linux/man-pages/man2/accept.2.html

### How AI was used

Claude (Anthropic) was used as a learning and debugging assistant throughout the project:

- **Understanding concepts**: HTTP protocol, CGI communication, non-blocking I/O, poll() event loop, cookie/session mechanics, path traversal attacks.
- **Debugging**: Identifying bugs such as the missing `POLLHUP` check that caused CGI responses to never arrive, the `cgi_last_activity` uninitialized value causing false timeouts, and the incomplete body reception before request processing.
- **Code review**: Checking compliance with subject requirements (no `errno` after read/write, single poll loop, non-blocking behavior).
- **Test design**: Writing curl commands and Python stress test scripts to validate each feature.

All AI-generated suggestions were reviewed, understood, and validated by the team before integration.
