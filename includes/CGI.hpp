/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CGI.hpp                                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: hguo <hguo@student.42.fr>                  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/03/30 16:53:55 by hguo              #+#    #+#             */
/*   Updated: 2026/03/30 16:53:56 by hguo             ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGI_HPP
# define CGI_HPP

# include "Http.hpp"
# include "ConfigParser.hpp"
# include "Client.hpp"

void startCGI(const HttpRequest &req, const LocationConfig &loc, ClientState &client);

# endif