#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include "ServerLocation.hpp"
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unistd.h>

class ServerConfig {
private:
    std::vector<int>               _ports;                   
    std::string                    _root;
    std::string                    _index;
    std::map<int, std::string>     _error_pages;
    std::vector<ServerLocation>    _locations;
    std::string                    _serverName;
    std::string                    _host;
    size_t                         _clientMaxBodySize;
    std::string rawBlock;
public:
    // Default constructor
    ServerConfig();
    void parseServerBlock(const std::string& serverBlock);
    void parseLocationBlock(const std::string& locationBlock, ServerLocation& location);
    void handleErrorPageDirective(const std::string& line);
    void handleLocationDirective(const std::string& line, const std::string& serverBlock, size_t& pos);

    void print() const;
    void clear();



    // Getters and Setters
    void setPort(int serverPort);
    const std::vector<int>& getPorts() const;

    size_t getClientMaxBodySize() const;
    void setClientMaxBodySize(size_t size);

    void setRoot(const std::string& rootPath);
    const std::string& getRoot() const;

    void setIndex(const std::string& indexPage);
    const std::string& getIndex() const;

    void setErrorPage(int code, const std::string& path);
    std::string getErrorPage(int errorCode) const;

    void setServerName(const std::string& name);
    const std::string& getServerName(void);

    void setHost(const std::string& host);
    const std::string& getHost() const;

    void addLocation(const ServerLocation& location);
    const std::vector<ServerLocation>& getLocations() const;

	int	getValid() const;
    std::string toString() const;


    const std::vector<std::string>& getServerBlocks() const;
    std::string trim(const std::string& str);

    bool isValidIP(const std::string& ip) const;

	
};

#endif
