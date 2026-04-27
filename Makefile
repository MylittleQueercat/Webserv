# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: hguo <hguo@student.42.fr>                  +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2026/03/30 16:52:36 by hguo              #+#    #+#              #
#    Updated: 2026/04/27 16:39:44 by hguo             ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME = Webserv

SRC_DIR = src
OBJ_DIR = obj

SRCS = $(SRC_DIR)/ConfigParser.cpp \
       $(SRC_DIR)/HandleHttpRequest.cpp \
       $(SRC_DIR)/HttpRequest.cpp \
       $(SRC_DIR)/runServer.cpp \
       $(SRC_DIR)/Server.cpp \
       $(SRC_DIR)/CGI.cpp \
       main.cpp

OBJS = $(addprefix $(OBJ_DIR)/, $(SRCS:.cpp=.o))
DEPS = $(OBJS:.o=.d)

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -I includes -MMD -MP

RESET  = \033[0m
BOLD   = \033[1m
DIM    = \033[2m
RED    = \033[31m
GREEN  = \033[32m
YELLOW = \033[33m
BLUE   = \033[34m
MAG    = \033[35m
CYAN   = \033[36m
WHITE  = \033[37m

all: $(NAME)

$(NAME): $(OBJS)
	@printf "$(CYAN)"
	@printf "╔══════════════════════════════════════════════════════════════════════════════╗\n"
	@printf "║                                                                              ║\n"
	@printf "║          ██╗    ██╗███████╗██████╗ ███████╗███████╗██████╗ ██╗   ██╗         ║\n"
	@printf "║          ██║    ██║██╔════╝██╔══██╗██╔════╝██╔════╝██╔══██╗██║   ██║         ║\n"
	@printf "║          ██║ █╗ ██║█████╗  ██████╔╝███████╗█████╗  ██████╔╝██║   ██║         ║\n"
	@printf "║          ██║███╗██║██╔══╝  ██╔══██╗╚════██║██╔══╝  ██╔══██╗╚██╗ ██╔╝         ║\n"
	@printf "║          ╚███╔███╔╝███████╗██████╔╝███████║███████╗██║  ██║ ╚████╔╝          ║\n"
	@printf "║           ╚══╝╚══╝ ╚══════╝╚═════╝ ╚══════╝╚══════╝╚═╝  ╚═╝  ╚═══╝           ║\n"
	@printf "║                                                                              ║\n"
	@printf "║                                      $(MAG)$(BOLD)42$(CYAN)                                      ║\n"
	@printf "║                               $(GREEN)hguo       jili$(CYAN)                                ║\n"
	@printf "║                                                                              ║\n"
	@printf "╚══════════════════════════════════════════════════════════════════════════════╝\n"
	@printf "$(RESET)"
	@printf "$(DIM)Boot sequence: [$(RESET)"
	@for i in 1 2 3 4 5 6 7 8 9 10 11 12; do \
		printf "$(GREEN)█$(RESET)"; \
		sleep 0.015; \
	done
	@printf "$(DIM)] ready$(RESET)\n\n"
	
# 	@printf "\n$(MAG)╔════════════════════════════════════════════════════════════════════╗$(RESET)\n"
# 	@printf "$(MAG)║$(RESET)  $(BOLD)LINKING EXECUTABLE$(RESET)                                                $(MAG)║$(RESET)\n"
# 	@printf "$(MAG)║$(RESET)  $(CYAN)$(NAME)$(RESET)                                                           $(MAG)║$(RESET)\n"
# 	@printf "$(MAG)╚════════════════════════════════════════════════════════════════════╝$(RESET)\n"
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
# 	@printf "\n$(GREEN)"
# 	@printf "        ╭────────────────────────────────────────────╮\n"
# 	@printf "        │  HTTP/1.1 200 OK                           │\n"
# 	@printf "        │  Server binary is ready: %-18s│\n" "$(NAME)"
# 	@printf "        ╰────────────────────────────────────────────╯\n"
# 	@printf "$(RESET)\n"

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@printf "$(BLUE)  GET$(RESET) %-42s " "$<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@
	@printf "$(GREEN)200 OK$(RESET)\n"

clean:
	@rm -rf $(OBJ_DIR)
	@printf "$(YELLOW)"
	@printf "╔════════════════════════════════════════════════════════════════════╗\n"
	@printf "║  DELETE /obj                                      204 No Content   ║\n"
	@printf "╚════════════════════════════════════════════════════════════════════╝\n"
	@printf "$(RESET)"

fclean: clean
	@rm -f $(NAME)
	@printf "$(RED)"
	@printf "╔════════════════════════════════════════════════════════════════════╗\n"
	@printf "║  DELETE /Webserv                                  204 No Content   ║\n"
	@printf "╚════════════════════════════════════════════════════════════════════╝\n"
	@printf "$(RESET)"

re: fclean all

-include $(DEPS)

.PHONY: all clean fclean re banner