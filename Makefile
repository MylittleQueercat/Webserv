# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: hguo <hguo@student.42.fr>                  +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2026/03/30 16:52:36 by hguo              #+#    #+#              #
#    Updated: 2026/03/30 16:52:37 by hguo             ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME = Webserv

SRC_DIR = src
OBJ_DIR = obj
SRCS = $(SRC_DIR)/ConfigParser.cpp \
		$(SRC_DIR)/HandleHttpRequest.cpp\
		$(SRC_DIR)/HttpRequest.cpp\
		$(SRC_DIR)/runServer.cpp\
		$(SRC_DIR)/Server.cpp\
		$(SRC_DIR)/CGI.cpp\
		main.cpp
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o) #$(SRCS:.cpp=.o)
CXX=c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -I includes

all: $(NAME)

$(NAME):$(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o:$(SRC_DIR)/%.cpp
	mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@
clean:
	rm -rf $(OBJ_DIR)
fclean: clean
	rm -f $(NAME)
re: fclean all