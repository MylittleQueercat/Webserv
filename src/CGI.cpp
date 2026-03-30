#include "../includes/CGI.hpp"
#include "../includes/Client.hpp"
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sstream>
#include <fcntl.h>

void startCGI(const HttpRequest &req, const LocationConfig &loc, ClientState &client) {
    (void)loc;
    int input_pipe[2];
    int output_pipe[2];
    pipe(input_pipe);
    pipe(output_pipe);

    pid_t pid = fork();

    if (pid == 0) {
        // 次进程：跟之前一样
        close(input_pipe[1]);
        close(output_pipe[0]);
        dup2(input_pipe[0], STDIN_FILENO);
        dup2(output_pipe[1], STDOUT_FILENO);
        close(input_pipe[0]);
        close(output_pipe[1]);

        std::string scriptpath = "./www" + req.path;

        // ✅ 根据扩展名选解释器
        std::string interpreter;
        std::string interpreter_path;
        if (req.path.find(".py") != std::string::npos) {
            interpreter      = "python3";
            interpreter_path = "/usr/bin/python3";
        } else if (req.path.find(".php") != std::string::npos) {
            interpreter      = "php";
            interpreter_path = "/usr/bin/php";
        } else if (req.path.find(".sh") != std::string::npos) {
            interpreter      = "bash";
            interpreter_path = "/bin/bash";
        } else {
            exit(1);  // 不支持的类型
        }

        // std::string scriptpath = loc.root + req.path;
        char *args[3];
        args[0] = (char*)interpreter.c_str();
        args[1] = (char*)scriptpath.c_str();
        args[2] = NULL;

        std::string method = "REQUEST_METHOD=" + req.method;
        std::string path   = "PATH_INFO=" + req.path;
        std::string query  = "QUERY_STRING=";
        char *env[4];
        env[0] = (char*)method.c_str();
        env[1] = (char*)path.c_str();
        env[2] = (char*)query.c_str();
        env[3] = NULL;

        execve(interpreter_path.c_str(), args, env);
        exit(1);
    }
    else {
        // 主进程：不等待！只存状态
        close(input_pipe[0]);

        // 写 POST body
        if (!req.body.empty())
            write(input_pipe[1], req.body.c_str(), req.body.size());
        close(input_pipe[1]);

        close(output_pipe[1]);

        // 存进 ClientState
        client.cgi_pid       = pid;
        client.cgi_output_fd = output_pipe[0];
        client.is_cgi        = true;
    }
}

// std::string executeCGI(const HttpRequest &req, const LocationConfig &loc) {

//     (void)loc;

//     int input_pipe[2];
//     int output_pipe[2];

//     pipe(input_pipe);
//     pipe(output_pipe);

//     pid_t pid = fork();

//     if (pid == 0) {
//         close(input_pipe[1]);
//         close(output_pipe[0]);

//         dup2(input_pipe[0], STDIN_FILENO);
//         dup2(output_pipe[1], STDOUT_FILENO);

//         close(input_pipe[0]);
//         close(output_pipe[1]);

//         // 构建脚本路径
//         std::string scriptpath = "./www" + req.path;

//         //参数
//         char *args[3];
//         args[0] = (char*)"python3";
//         args[1] = (char*)scriptpath.c_str();
//         args[2] = NULL;

//         // 环境变量
//         std::string method = "REQUEST_METHOD=" + req.method;
//         std::string path   = "PATH_INFO=" + req.path;
//         std::string query  = "QUERY_STRING=";
//         char *env[4];
//         env[0] = (char*)method.c_str();
//         env[1] = (char*)path.c_str();
//         env[2] = (char*)query.c_str();
//         env[3] = NULL;

//         // 运行脚本
//         execve("/usr/bin/python3", args, env);
//         exit(1);
//     }
//     else {
//         close(input_pipe[0]);
//         close(output_pipe[1]);

//         //把POST body写进 input_pipe
//         if (!req.body.empty())
//             write(input_pipe[1], req.body.c_str(), req.body.size());
//         close(input_pipe[1]);

//         // 等待次进程结束
//         waitpid(pid, NULL, 0);

//         // 从 output_pipe读取脚本输出
//         std::string output;
//         char buffer[4096];
//         int bytes;
//         while ((bytes = read(output_pipe[0], buffer, sizeof(buffer))) > 0)
//             output += std::string(buffer, bytes);
//         close(output_pipe[0]);

//         // 构建http响应
//         std::string body_only = output.substr(output.find("\r\n\r\n") + 4);
//         std::string headers = output.substr(0, output.find("\r\n\r\n") + 4);

//         std::ostringstream oss;
//         oss << body_only.size();

//         // headers 里已经有 \r\n\r\n，要去掉最后的 \r\n\r\n
//         std::string headers_only = output.substr(0, output.find("\r\n\r\n"));

//         std::string response = "HTTP/1.1 200 OK\r\n";
//         response += "Connection: close\r\n"; // 连接关闭
//         response += headers_only + "\r\n";  // 头部，不包含最后的空行
//         response += "Content-Length: " + oss.str() + "\r\n";
//         response += "\r\n";  // 最后的空行
//         response += body_only;
//         return response;
//     }
// }