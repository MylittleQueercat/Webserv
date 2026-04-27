/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGI.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hguo <hguo@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/30 16:53:55 by hguo              #+#    #+#             */
/*   Updated: 2026/04/27 10:49:18 by hguo             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGI_HPP
# define CGI_HPP

# include "Http.hpp"
# include "ConfigParser.hpp"
# include "Client.hpp"
void cleanupCGI(ClientState& client,
                       std::vector<struct pollfd>& fds,
                       size_t& i);
void startCGI(const HttpRequest &req, const LocationConfig &loc, ClientState &client, bool keep_stdin_open);

# endif