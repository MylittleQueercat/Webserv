/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Webserv.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hguo <hguo@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/30 16:54:08 by hguo              #+#    #+#             */
/*   Updated: 2026/03/30 16:54:09 by hguo             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef WEBSERV_HPP
# define WEBSERV_HPP

#include <poll.h>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <set>
#include <sstream>
#include <sys/wait.h>
#include <ctime>
#include <cstdlib>
#include <stdlib.h>
#include <fcntl.h>
#include <string>
#include <linux/limits.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "Client.hpp"
#include "Http.hpp"
#include "CGI.hpp"
#include "ConfigParser.hpp"
#include "Server.hpp"

void runServer(std::vector<ServerConfig> &configs);

#endif