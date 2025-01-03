#include "ServerConfig.hpp"

ServerConfig::ServerConfig() : _root("var/www/main"), _index("index.html"), _host("127.0.0.1"), _clientMaxBodySize(100000000)
{
    setErrorPage(404, ("main/errors/404.html"));
    setErrorPage(500, ("main/errors/500.html"));
    setErrorPage(504, ("main/errors/504.html"));
    setErrorPage(411, ("main/errors/411.html"));
    setErrorPage(400, ("main/errors/400.html"));
    setErrorPage(403, ("main/errors/403.html"));
    setErrorPage(405, ("main/errors/405.html"));
    setErrorPage(413, ("main/errors/413.html"));
    setErrorPage(415, ("main/errors/415.html"));
}

void ServerConfig::parseServerBlock(const std::string& serverBlock)
{
    size_t pos = 0;
    size_t end;

    bool hasListen = false;
    bool hasRoot = false;

    while (pos < serverBlock.size())
    {
        end = serverBlock.find('\n', pos);
        if (end == std::string::npos)
            end = serverBlock.size();

        std::string line = serverBlock.substr(pos, end - pos);

        line.erase(0, line.find_first_not_of(" \t\r"));
        line.erase(line.find_last_not_of(" \t\r") + 1);

        if (line.empty() || line[0] == '#' || line == "server {" || line == "}") 
        {
            pos = end + 1;
            continue;
        }

        if (line.find("listen") == 0)
        {
            std::string value = line.substr(6);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t;") + 1);

            if (value.empty())
                throw std::runtime_error("Error: Missing value for 'listen'");

            int port = std::atoi(value.c_str());
            if (port <= 0 || port > 65535)
                throw std::runtime_error("Error: Invalid port value '" + value + "'");

            _ports.push_back(port);
            hasListen = true;
        }
        else if (line.find("root") == 0)
        {
            std::string value = line.substr(4);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t;") + 1);

            if (value.empty())
                throw std::runtime_error("Error: Missing value for 'root'");

            _root = value;
            hasRoot = true;
        }
        else if (line.find("index") == 0)
        {
            std::string value = line.substr(5);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t;") + 1);

            if (value.empty())
                throw std::runtime_error("Error: Missing value for 'index'");

            _index = value;
        }
        else if (line.find("server_name") == 0)
        {
            std::string value = line.substr(11);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t;") + 1);

            if (value.empty())
                throw std::runtime_error("Error: Missing value for 'server_name'");

            _serverName = value;
        }
        else if (line.find("host") == 0)
        {
            std::string value = line.substr(4);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t;") + 1);

            if (value.empty())
                throw std::runtime_error("Error: Missing value for 'host'");

            setHost(value);
        }
        else if (line.find("client_max_body_size") == 0)
        {
            std::string value = line.substr(20);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t;") + 1);

            if (value.empty())
                throw std::runtime_error("Error: Missing value for 'client_max_body_size'");

            _clientMaxBodySize = std::strtoul(value.c_str(), NULL, 10);
        }
        else if (line.find("location") == 0)
        {
            handleLocationDirective(line, serverBlock, pos);
            continue;
        }
        else if (line.find("error_page") == 0)
            handleErrorPageDirective(line);
        else
            throw std::runtime_error("Error: Unknown directive '" + line + "'");

        pos = end + 1;
    }

    if (!hasListen)
        throw std::runtime_error("Error: Missing 'listen' directive in server block");
    if (!hasRoot)
        throw std::runtime_error("Error: Missing 'root' directive in server block");
}

void ServerConfig::handleLocationDirective(const std::string& line, const std::string& serverBlock, size_t& pos)
{
    size_t pathStart = line.find("location") + 8;
    std::string path = line.substr(pathStart);
    path.erase(0, path.find_first_not_of(" \t"));
    size_t pathEnd = path.find_first_of(" \t{");
    if (pathEnd != std::string::npos)
        path = path.substr(0, pathEnd);

    if (path.empty())
        throw std::runtime_error("Error: Missing 'path' for location block");
    size_t locationStart = serverBlock.find('{', pos);
    size_t locationEnd = serverBlock.find('}', locationStart);

    if (locationStart == std::string::npos || locationEnd == std::string::npos)
        throw std::runtime_error("Error: Malformed location block");

    std::string locationBlock = serverBlock.substr(locationStart + 1, locationEnd - locationStart - 1);

    ServerLocation location(path);

    try
    {
        parseLocationBlock(locationBlock, location);
        _locations.push_back(location);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Warning: Invalid location block ignored. " << e.what() << std::endl;
    }

    pos = locationEnd + 1;
}

void ServerConfig::handleErrorPageDirective(const std::string& line)
{
    std::string value = line.substr(10);
    value.erase(0, value.find_first_not_of(" \t"));
    value.erase(value.find_last_not_of(" \t;") + 1);

    if (value.empty())
        throw std::runtime_error("Error: Missing value for 'error_page'");

    size_t spacePos = value.find(' ');
    if (spacePos == std::string::npos)
        throw std::runtime_error("Error: Invalid format for 'error_page'. Expected: <code> <path>");

    std::string errorCodeStr = value.substr(0, spacePos);
    errorCodeStr.erase(0, errorCodeStr.find_first_not_of(" \t"));
    errorCodeStr.erase(errorCodeStr.find_last_not_of(" \t") + 1);

    int errorCode = std::atoi(errorCodeStr.c_str());
    if (errorCode < 100 || errorCode > 599)
        throw std::runtime_error("Error: Invalid error code '" + errorCodeStr + "' for 'error_page'");

    std::string errorPath = value.substr(spacePos + 1);
    errorPath.erase(0, errorPath.find_first_not_of(" \t"));
    errorPath.erase(errorPath.find_last_not_of(" \t;") + 1);
    if (errorPath.empty())
        throw std::runtime_error("Error: Missing path for 'error_page'");
    std::string fullPath = _root + errorPath;
    std::ifstream testFile(fullPath.c_str());
    if (!testFile.is_open())
        throw std::runtime_error("Error page file does not exist: " + fullPath);
    testFile.close();

    _error_pages[errorCode] = errorPath;
}

void ServerConfig::parseLocationBlock(const std::string& locationBlock, ServerLocation& location)
{
    size_t pos = 0;
    size_t end;

    while (pos < locationBlock.size())
    {
        end = locationBlock.find('\n', pos);
        if (end == std::string::npos)
            end = locationBlock.size();

        std::string line = locationBlock.substr(pos, end - pos);

        line.erase(0, line.find_first_not_of(" \t\r"));
        line.erase(line.find_last_not_of(" \t\r;") + 1);

        if (line.empty() || line[0] == '#')
        {
            pos = end + 1;
            continue;
        }

        if (line.find("root") == 0)
        {
            std::string value = line.substr(4);
            value.erase(0, value.find_first_not_of(" \t"));
            location.setRoot(value);

            std::ifstream testFile(value.c_str());
            if (!testFile.is_open())
            throw std::runtime_error("The specified root directory does not exist: " + value);
        }
        else if (line.find("index") == 0)
        {
            std::string value = line.substr(5);
            value.erase(0, value.find_first_not_of(" \t"));
            location.setIndex(value);
            std::string fullPath = location.getRoot() + value;
            std::ifstream testFile(fullPath.c_str());
            if (!testFile.is_open())
                throw std::runtime_error("The specified index file does not exist " + fullPath);
        }
        else if (line.find("methods") == 0)
        {
            location.disableAllMethods();
            std::string methods = line.substr(7);
            methods.erase(0, methods.find_first_not_of(" \t"));

            if (methods.find("GET") != std::string::npos)
                location.allowGet();
            if (methods.find("POST") != std::string::npos)
                location.allowPost();
            if (methods.find("DELETE") != std::string::npos)
                location.allowDelete();
        }
        else
        {
            if (line.find_first_not_of(" \t") == std::string::npos)
            {
                pos = end + 1;
                continue;
            }
            std::cerr << "Unknown directive: [" << line << "]" << std::endl;

            throw std::runtime_error("Error: Unknown directive in location block: '" + line + "'");
        }

        pos = end + 1;
    }
}

void ServerConfig::clear()
{
    _ports.clear();
    _root.clear();
    _index.clear();
    _error_pages.clear();
    _locations.clear();
    _serverName.clear();
    _host.clear();
    _clientMaxBodySize = 0;
}

void ServerConfig::print() const
{
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "               Config                   " << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    std::cout << "Ports: ";
    for (size_t i = 0; i < _ports.size(); ++i)
    {
        if (i > 0) std::cout << ", ";
        std::cout << _ports[i];
    }
    std::cout << std::endl;

    std::cout << "Root: " << _root << std::endl;
    std::cout << "Index: " << _index << std::endl;
    std::cout << "Server Name: " << _serverName << std::endl;
    std::cout << "Host: " << _host << std::endl;
    std::cout << "Client Max Body Size: " << _clientMaxBodySize << std::endl;

    std::cout << "Error Pages: " << std::endl;
    for (std::map<int, std::string>::const_iterator it = _error_pages.begin(); it != _error_pages.end(); ++it)
    {
        std::cout << "  Error " << it->first << ": " << it->second << std::endl;
    }

    std::cout << "Locations: " << std::endl;
    for (size_t i = 0; i < _locations.size(); ++i)
    {
        _locations[i].display();
    }
    std::cout << std::endl;
}

