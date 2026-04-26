/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Http.hpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hguo <hguo@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/30 17:03:44 by jili              #+#    #+#             */
/*   Updated: 2026/04/07 16:22:11 by hguo             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTP_HPP
# define HTTP_HPP

# include <string>
# include <map>
# include "ConfigParser.hpp"

struct HttpRequest {
    std::string method;   // "GET"
    std::string path;     // "/index.html"
    std::string version;  // "HTTP/1.1"
    std::map<std::string, std::string> headers;
    std::string body;
    int         client_fd;
};
std::string buildErrorResponse(int code, const ServerConfig &config);
//std::string buildErrorResponse(int code, const std::string &error_page_path);
HttpRequest parseRequest(const std::string &raw);
std::string handleRequest(const HttpRequest &req, const ServerConfig &config, const LocationConfig &loc);
std::string unchunk(const std::string &body);
#endif