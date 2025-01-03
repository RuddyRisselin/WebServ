/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ymostows <ymostows@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/10/22 16:44:32 by ymostows          #+#    #+#             */
/*   Updated: 2024/10/22 16:44:32 by ymostows         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "HttpRequest.hpp"
#include "ServerConfig.hpp"

volatile sig_atomic_t Server::signal_received = 0;

int Server::createSocket()
{
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
        throw std::runtime_error(logMessageError("ERROR", "Failed to create socket."));
    return server_fd;
}

void Server::initSockets()
{
    std::vector<std::pair<std::string, int> > boundSockets;

    for (size_t i = 0; i < _configs.size(); ++i)
    {
        const std::vector<int>& ports = _configs[i].getPorts();
        const std::string& host = _configs[i].getHost();

        for (size_t j = 0; j < ports.size(); ++j)
        {
            std::pair<std::string, int> socketKey = std::make_pair(host, ports[j]);

            if (std::find(boundSockets.begin(), boundSockets.end(), socketKey) != boundSockets.end()) {
                logMessage("INFO", "Socket already bound for " + host + ":" + intToString(ports[j]));
                continue;
            }

            int server_fd = createSocket();
            try {
                configureSocket(server_fd);

                sockaddr_in address = {};
                address.sin_family = AF_INET;
                address.sin_addr.s_addr = inet_addr(host.c_str());
                address.sin_port = htons(ports[j]);

                if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0)
                {
                    close(server_fd);
                    _server_fds.erase(std::remove(_server_fds.begin(), _server_fds.end(), server_fd), _server_fds.end());
                    throw std::runtime_error("Failed to bind socket for host: " + host + " on port " + intToString(ports[j]));
                }

                boundSockets.push_back(socketKey);
                _addresses.push_back(address);
                listenOnSocket(server_fd);
                _socketToConfig[server_fd] = &_configs[i];
                addServerSocketToPoll(server_fd);
                logMessage("INFO", "Server is listening on " + host + ":" + intToString(ports[j]));
            } 
            catch (const std::exception& e)
            {
                close(server_fd);
                logMessage("ERROR", e.what());
                continue;
            }
        }
    }
}


void Server::configureSocket(int server_fd)
{
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        close(server_fd);
        throw std::runtime_error(logMessageError("ERROR", "Failed to configure socket options (SO_REUSEADDR)."));
    }
}

void Server::listenOnSocket(int server_fd)
{
    if (listen(server_fd, 10) < 0)
    {
        close(server_fd);
        throw std::runtime_error(logMessageError("ERROR", "Failed to set socket to listen."));
    }
}

void Server::addServerSocketToPoll(int server_fd)
{
    struct pollfd server_poll_fd = {};
    server_poll_fd.fd = server_fd;
    server_poll_fd.events = POLLIN | POLLOUT;
    _poll_fds.push_back(server_poll_fd);
    _server_fds.push_back(server_fd);
}

ServerConfig* Server::getConfigForRequest(const std::string& hostHeader, int connectedPort)
{
    if (hostHeader.empty())
        return &_configs[0];
    std::string hostWithoutPort = hostHeader;
    int portFromHeader = connectedPort;
    size_t colonPos = hostHeader.find(':');
    if (colonPos != std::string::npos)
    {
        hostWithoutPort = hostHeader.substr(0, colonPos);
        std::istringstream ss(hostHeader.substr(colonPos + 1));
        ss >> portFromHeader;
    }
    ServerConfig* defaultForPort = NULL;
    for (size_t i = 0; i < _configs.size(); ++i)
    {
        const std::string& configHost = _configs[i].getHost();
        const std::string& configServerName = _configs[i].getServerName();
        const std::vector<int>& configPorts = _configs[i].getPorts();
        if (!configServerName.empty()) {
            if (hostWithoutPort == configServerName) {
                if (std::find(configPorts.begin(), configPorts.end(), portFromHeader) != configPorts.end())
                    return &_configs[i];
            }
        }
        if (hostWithoutPort == configHost) {
            if (std::find(configPorts.begin(), configPorts.end(), portFromHeader) != configPorts.end())
                return &_configs[i];
        }
        if (std::find(configPorts.begin(), configPorts.end(), portFromHeader) != configPorts.end() && !defaultForPort)
            defaultForPort = &_configs[i];
    }
    if (defaultForPort)
        return defaultForPort;
    return NULL;
}


void Server::cleanupSockets()
{
    for (size_t i = 0; i < _server_fds.size(); ++i)
    {
        if (_server_fds[i] != -1)
        {
            close(_server_fds[i]);
            _server_fds[i] = -1;
        }
    }
    _poll_fds.clear();
}

void Server::run()
{
    logMessage("INFO", "Server is running...");
    running = true;

    while (running)
    {
        int poll_count = poll(&_poll_fds[0], _poll_fds.size(), -1);

        if (poll_count < 0)
        {
            if (!running)
                break;
            logMessage("ERROR", "Poll failed.");
            continue;
        }

        for (size_t i = 0; i < _poll_fds.size(); ++i)
        {
            if (_poll_fds[i].revents & POLLIN)
            {
                if (isServerSocket(_poll_fds[i].fd))
                    handleNewConnection(_poll_fds[i].fd);
                else
                    handleClientRequest(i);
            }
            if (_poll_fds[i].revents & POLLOUT)
            {
                int client_fd = _poll_fds[i].fd;
                if (responseBuffer.find(client_fd) != responseBuffer.end())
                {
                    const std::string& response = responseBuffer[client_fd];
                    ssize_t bytes_sent = send(client_fd, response.c_str(), response.size(), 0);
                    
                    if (bytes_sent == -1 || bytes_sent == 0)
                    {
                        logMessage("ERROR", "Failed to send data to client " + intToString(client_fd));
                        removeClient(i);
                    }
                    else
                    {
                        responseBuffer.erase(client_fd);
                        _poll_fds[i].events &= ~POLLOUT;
                    }
                }
            }
        }
    }
}


bool Server::isServerSocket(int fd) const
{
    return std::find(_server_fds.begin(), _server_fds.end(), fd) != _server_fds.end();
}

void Server::handleNewConnection(int server_fd)
{
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);

    if (client_fd < 0)
    {
        logMessage("ERROR", "Failed to accept new connection.");
        return;
    }

    struct pollfd client_poll_fd = {};
    client_poll_fd.fd = client_fd;
    client_poll_fd.events = POLLIN;

    _poll_fds.push_back(client_poll_fd);

    ServerConfig* config = _socketToConfig[server_fd];
    if (config)
        _socketToConfig[client_fd] = config;

    else
        logMessage("WARNING", "Could not find server configuration for client.");
}


void Server::handleClientRequest(int clientIndex)
{
    int client_fd = _poll_fds[clientIndex].fd;
    std::string buffer = readClientRequest(client_fd, clientIndex);
    if (buffer.empty())
        return;
    HttpRequest request(buffer);
    std::string hostHeader = request.getHeaderValue("Host");
    int connectedPort = -1;
    struct sockaddr_in addr;
    socklen_t addrLen = sizeof(addr);
    if (getsockname(client_fd, (struct sockaddr*)&addr, &addrLen) == 0)
        connectedPort = ntohs(addr.sin_port);
    ServerConfig* config = getConfigForRequest(hostHeader, connectedPort);
    if (!config)
    {
        logMessage("ERROR", "No configuration found for client " + intToString(client_fd));
        removeClient(clientIndex);
        return;
    }
    try {
        std::string response = request.handleRequest(*config);
         logMessage("INFO", request.getMethod() + " " + request.getPath() + " " + request.getHttpVersion() + + "\" " + intToString(request.extractStatusCode(response)) + " " + intToString(response.size()) + " \"" + request.getHeaderValue("User-Agent") + "\"");
        responseBuffer[client_fd] = response;
        _poll_fds[clientIndex].events |= POLLOUT;
    }
    catch (const std::exception& e) {
        logMessage("ERROR", "Failed to handle request for client " + intToString(client_fd));
        removeClient(clientIndex);
    }
}

std::string Server::readClientRequest(int client_fd, int clientIndex)
{
    char tempBuffer[1024];
    ssize_t bytes_read;

    bytes_read = recv(client_fd, tempBuffer, sizeof(tempBuffer) - 1, MSG_DONTWAIT);

    if (bytes_read > 0)
    {
        tempBuffer[bytes_read] = '\0';
        clientBuffers[client_fd] += std::string(tempBuffer, bytes_read);

        if (clientBuffers[client_fd].find("\r\n\r\n") != std::string::npos || clientBuffers[client_fd].find("\n\n") != std::string::npos)
        {
            size_t contentLengthPos = clientBuffers[client_fd].find("Content-Length:");
            
            if (contentLengthPos != std::string::npos) {
                size_t start = clientBuffers[client_fd].find(" ", contentLengthPos) + 1;
                size_t end = clientBuffers[client_fd].find("\r\n", contentLengthPos);
                int contentLength = std::atoi(clientBuffers[client_fd].substr(start, end - start).c_str());
                size_t currentBodySize = clientBuffers[client_fd].size() - clientBuffers[client_fd].find("\r\n\r\n") - 4;
                if (currentBodySize >= static_cast<size_t>(contentLength))
                {
                    std::string completeRequest = clientBuffers[client_fd];
                    clientBuffers.erase(client_fd);
                    return completeRequest;
                }
            }
            else
            {
                std::string completeRequest = clientBuffers[client_fd];
                clientBuffers.erase(client_fd);
                return completeRequest;
            }
        }
    }
    else if (bytes_read == 0)
        removeClient(clientIndex);
    else if (bytes_read == -1)
    {
        logMessage("ERROR", "Read error on client socket." + intToString(client_fd));
        removeClient(clientIndex);
    }
    return "";
}

void Server::removeClient(int index)
{
    int client_fd = _poll_fds[index].fd;

    if (responseBuffer.find(client_fd) != responseBuffer.end())
        responseBuffer.erase(client_fd);
    if (_socketToConfig.find(client_fd) != _socketToConfig.end())
        _socketToConfig.erase(client_fd);
    if (client_fd != -1)
        close(client_fd);
    _poll_fds.erase(_poll_fds.begin() + index);
}

void Server::stop()
{
    logMessage("INFO", "Stopping the server...");
    running = false;
    cleanupSockets();
    logMessage("INFO", "Server stopped successfully.");
}

void Server::signalHandler(int signal)
{
    if (signal == SIGINT)
        signal_received = 1;
}