#include "Server.hpp"
#include "HttpRequest.hpp"
#include "ServerConfig.hpp"

Server::Server(const std::string configFile) : running(false)
{
    logMessage("INFO", "Initializing the server...");
    try
    {
        if (!parseConfigFile(configFile))
            throw std::runtime_error("Failed to parse configuration file: " + configFile);
        validateServerConfigurations();
        if (_configs.empty())
            throw std::runtime_error("Failed to parse configuration file: 0 valid config");
        initSockets();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error during server initialization: " << e.what() << std::endl;
        cleanup();
        throw;
    }
}

Server::~Server()
{
    cleanupSockets();
}

void Server::cleanup()
{
    logMessage("INFO", "Cleaning up resources...");
    _configs.clear();
    cleanupSockets();
}

bool Server::parseConfigFile(std::string configFile)
{
    if (parseFileInBlock(configFile) == false)
        return false;

    for (std::vector<std::string>::iterator it = serverBlocks.begin(); it != serverBlocks.end(); ++it)
    {
        ServerConfig config;
        try
        {
            config.parseServerBlock(*it);
            _configs.push_back(config);
        }
        catch (const std::runtime_error& e)
        {
            std::cerr << "[ERROR] parsing server block failed : " << e.what() << std::endl;
            config.clear();
        }
    }

    return !_configs.empty();
}

void Server::validateServerConfigurations()
{
    for (size_t i = 0; i < _configs.size(); ++i)
    {
        const std::string& host1 = _configs[i].getHost();
        const std::vector<int>& ports1 = _configs[i].getPorts();
        const std::string& serverName1 = _configs[i].getServerName();

        bool shouldEraseI = false;  // Déclaration ici (en dehors de la boucle j)

        for (size_t j = i + 1; j < _configs.size();)
        {
            const std::string& host2 = _configs[j].getHost();
            const std::vector<int>& ports2 = _configs[j].getPorts();
            const std::string& serverName2 = _configs[j].getServerName();

            bool shouldEraseJ = false;

            for (size_t p1 = 0; p1 < ports1.size(); ++p1)
            {
                for (size_t p2 = 0; p2 < ports2.size(); ++p2)
                {
                    if (ports1[p1] == ports2[p2] && host1 == host2)
                    {
                        if (serverName1.empty() && serverName2.empty())
                        {
                            std::cout << "Multiple servers on the same host ("
                                      << host1 << ") and port (" << intToString(ports1[p1])
                                      << ") without server_name." << std::endl;
                            shouldEraseI = true;
                            shouldEraseJ = true;
                        }
                        else if (!serverName1.empty() && !serverName2.empty() && serverName1 == serverName2)
                        {
                            std::cout << "Duplicate server_name (" << serverName1
                                      << ") on the same host (" << host1
                                      << ") and port (" << intToString(ports1[p1]) << ")." << std::endl;
                            shouldEraseJ = true;
                        }
                    }
                }
            }
            if (shouldEraseJ)
            {
                _configs.erase(_configs.begin() + j);
                continue;  // Ne pas incrémenter j, car l'élément suivant prend sa place
            }
            ++j;  // Incrément si pas de suppression
        }
        if (shouldEraseI)
        {
            _configs.erase(_configs.begin() + i);
            --i;  // Revenir en arrière car l'élément i a été supprimé
        }
    }
}




bool Server::parseFileInBlock(std::string configFile)
{
    std::ifstream file(configFile.c_str());
    if (!file.is_open())
    {
        std::cerr << "Error: Unable to open config file: " << configFile << std::endl;
        return false;
    }

    std::string line;
    std::string currentBlock;
    bool inServerBlock = false;
    int braceCount = 0;

    while (std::getline(file, line))
    {
        std::string trimmedLine = line;
        trimmedLine.erase(0, trimmedLine.find_first_not_of(" \t"));
        trimmedLine.erase(trimmedLine.find_last_not_of(" \t") + 1);

        if (trimmedLine.empty() || trimmedLine[0] == '#')
            continue;

        if (trimmedLine.find("server {") == 0)
        {
            inServerBlock = true;
            braceCount = 1;
            currentBlock = "server {\n";
            continue;
        }

        if (inServerBlock)
        {
            currentBlock += line + "\n";
            for (size_t i = 0; i < line.length(); ++i)
            {
                if (line[i] == '{') ++braceCount;
                if (line[i] == '}') --braceCount;
            }
            if (braceCount == 0)
            {
                inServerBlock = false;
                serverBlocks.push_back(currentBlock);
                currentBlock.clear();
            }
            else if (braceCount < 0)
            {
                std::cerr << "Error: Mismatched braces in configuration file." << std::endl;
                return false;
            }
        }
    }

    if (inServerBlock)
    {
        std::cerr << "Error: Unclosed server block at end of file." << std::endl;
        return false;
    }
    return true;
}

void Server::logMessage(const std::string& level, const std::string& message) const
{
    std::time_t now = std::time(NULL);
    std::tm* localTime = std::localtime(&now);
    char timeBuffer[20];

    std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", localTime);
    std::cout << "[" << level << "] " << timeBuffer << " - " << message << std::endl;
}

std::string Server::logMessageError(const std::string& level, const std::string& message) const
{
    std::time_t now = std::time(NULL);
    std::tm* localTime = std::localtime(&now);

    char timeBuffer[20];
    std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", localTime);

    std::ostringstream oss;
    oss << "[" << level << "] " << timeBuffer << " - " << message;

    return oss.str();
}

void Server::logResponseDetails(const std::string& response, const std::string& path)
{
    size_t statusLineEnd = response.find("\r\n");
    if (statusLineEnd == std::string::npos)
    {
        logMessage("ERROR", "Malformed response for path: " + path);
        return;
    }

    std::string statusLine = response.substr(0, statusLineEnd);
    size_t statusCodeStart = statusLine.find(" ") + 1;
    size_t statusCodeEnd = statusLine.find(" ", statusCodeStart);
    std::string statusCode = statusLine.substr(statusCodeStart, statusCodeEnd - statusCodeStart);

    size_t contentTypeStart = response.find("Content-Type: ");
    std::string contentType = "Unknown";
    if (contentTypeStart != std::string::npos)
    {
        size_t contentTypeEnd = response.find("\r\n", contentTypeStart);
        contentType = response.substr(contentTypeStart + 14, contentTypeEnd - (contentTypeStart + 14));
    }
    logMessage("INFO", "Response sent: " + statusCode + " for " + path + " with Content-Type: " + contentType);
}

std::string Server::intToString(int value)
{
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

void Server::printServerBlocks() const
{
    if (serverBlocks.empty())
    {
        std::cout << "No server blocks available." << std::endl;
        return;
    }

    for (size_t i = 0; i < serverBlocks.size(); ++i) {
        std::cout << "Server Block " << i + 1 << ":\n";
        std::cout << serverBlocks[i] << std::endl;
        std::cout << "-----------------------------------" << std::endl;
    }
}