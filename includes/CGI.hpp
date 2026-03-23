#ifndef CGI_HPP
# define CGI_HPP

# include "Http.hpp"
# include "ConfigParser.hpp"
# include "Client.hpp"

void startCGI(const HttpRequest &req, const LocationConfig &loc, ClientState &client);

# endif