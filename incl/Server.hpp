/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ymostows <ymostows@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/10/22 16:45:45 by ymostows          #+#    #+#             */
/*   Updated: 2024/10/22 16:45:45 by ymostows         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <poll.h>
#include <sstream>
#include <algorithm>
#include <csignal>
#include <stdexcept>
#include <fstream>
#include <arpa/inet.h>
#include <ctime>
#include <memory>
#include <iomanip>
#include "ServerConfig.hpp"

class Server {
private:
    // Parsing
    bool parseConfigFile(std::string configFile);
    void printServerBlocks() const;
    bool parseFileInBlock(std::string configFile);

    // Sockets
    int createSocket();
    void configureSocket(int server_fd);
    void bindSocket(int server_fd, int port);
    void listenOnSocket(int server_fd);
    void addServerSocketToPoll(int server_fd);
    void cleanupSockets();
    void cleanup();
    bool isServerSocket(int fd) const;

    // Handle connections
    void handleNewConnection(int server_fd);
    void handleClientRequest(int clientIndex);
    void logResponseDetails(const std::string& response, const std::string& path);
    std::string readClientRequest(int client_fd, int clientIndex);
    void unchunk();
    std::string chunkedToBody(int client_fd, int clientIndex, std::string buffer, size_t transferEncodingPos);
    void removeClient(int index);
    void validateServerConfigurations();
    void displayConfigs(const std::vector<ServerConfig>& configs);
    
    // Variables
    bool running;
    std::vector<int> _server_fds;
    std::vector<int> _ports;
    std::vector<sockaddr_in> _addresses;
    std::vector<pollfd> _poll_fds;
    std::vector<std::string> serverBlocks;
    std::vector<ServerConfig> _configs;
    std::map<int, ServerConfig*> _socketToConfig;
    std::map<int, std::string> responseBuffer;
    std::map<int, std::string> clientBuffers;
    static volatile sig_atomic_t signal_received;
public:
    Server(const std::string configFile);
    ~Server();

    // Main fonctions
    void initSockets();
    void run();                      
    void stop();
    bool isRunning() const;
    void setRunning(bool status);

    static void signalHandler(int signal);

    // Utils and logMessages
    std::string intToString(int value);
    void logMessage(const std::string& level, const std::string& message) const;
    std::string logMessageError(const std::string& level, const std::string& message) const;

    ServerConfig* getConfigForSocket(int socket);
    ServerConfig* getConfigForRequest(const std::string& hostHeader, int connectedPort);
};

#endif // SERVER_HPP