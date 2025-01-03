# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: ymostows <ymostows@student.42.fr>          +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2024/10/22 16:15:46 by ymostows          #+#    #+#              #
#    Updated: 2024/10/22 16:15:46 by ymostows         ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

NAME = webserv

SRC_DIR = srcs
INC_DIR = incl
OBJ_DIR = objs
UPLOAD_DIR = var/www/upload

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -g3 -I$(INC_DIR)

SRCS = $(SRC_DIR)/main.cpp $(SRC_DIR)/Server.cpp $(SRC_DIR)/utilsServer.cpp $(SRC_DIR)/HttpRequest.cpp $(SRC_DIR)/ServerConfig.cpp $(SRC_DIR)/ServerLocation.cpp  $(SRC_DIR)/utilsRequest.cpp $(SRC_DIR)/utilsParsing.cpp
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

cleanupload:
	rm -rf $(UPLOAD_DIR)/*
	@echo "Uploads directory cleaned."

re: fclean all

.PHONY: all clean fclean re cleanupload
