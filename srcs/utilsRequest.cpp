#include "HttpRequest.hpp"

std::string HttpRequest::intToString(int value)
{
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

std::string HttpRequest::resolveFilePath(const ServerConfig& config)
{
    std::string fullPath;
    bool locationFound = false;
    const std::vector<ServerLocation>& locations = config.getLocations();
    for (std::vector<ServerLocation>::const_iterator it = locations.begin(); it != locations.end(); ++it)
    {
        if (this->_path == it->getPath())
        {
            fullPath = it->getRoot() + it->getIndex();
            locationFound = true;
            break;
        }
    }

    if (!locationFound)
    {
        if (this->_path == "/")
            fullPath = config.getRoot() + config.getIndex();
        else
        {
            fullPath = _path;
            fullPath = config.getRoot() + fullPath.substr(1);
        } 
    }
    return fullPath;
}

bool HttpRequest::isFileAccessible(const std::string& filePath)
{
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) != 0)
        return false;
    if (access(filePath.c_str(), R_OK) != 0)
        return false;
    return true;
}

std::string HttpRequest::readFile(const std::string& filePath)
{
    std::ifstream file(filePath.c_str(), std::ios::binary);
    if (!file.is_open())
        return "";
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size > 0 && size < std::numeric_limits<std::streamsize>::max())
    {
        std::vector<char> buffer(static_cast<size_t>(size));
        if (file.read(buffer.data(), size))
            return std::string(buffer.data(), size);
    }
    return "";
}

std::string HttpRequest::getMimeType(const std::string& filePath)
{
    std::map<std::string, std::string> mimeTypes;
    mimeTypes[".php"] = "text/html";
    mimeTypes[".html"] = "text/html";
    mimeTypes[".css"] = "text/css";
    mimeTypes[".js"] = "application/javascript";
    mimeTypes[".jpg"] = "image/jpeg";
    mimeTypes[".jpeg"] = "image/jpeg";
    mimeTypes[".png"] = "image/png";
    mimeTypes[".gif"] = "image/gif";
    mimeTypes[".svg"] = "image/svg+xml";
    mimeTypes[".ico"] = "image/x-icon";
    mimeTypes[".json"] = "application/json";
    mimeTypes[".xml"] = "application/xml";
    mimeTypes[".txt"] = "text/plain";
    mimeTypes[".py"] = "text/html";
    mimeTypes[".mp4"] = "video/mp4";

    size_t dotPos = filePath.find_last_of('.');
    if (dotPos != std::string::npos)
    {
        std::string extension = filePath.substr(dotPos);
        if (mimeTypes.count(extension) > 0)
            return mimeTypes[extension];
    }

    return "application/octet-stream";
}

std::string HttpRequest::constructCGIResponse(const std::string& output)
{
    std::ostringstream oss;
    oss << output.size();

    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Length: " + oss.str() + "\r\n";
    response += "Content-Type: text/html\r\n";
    response += "\r\n";
    response += output;

    return response;
}

std::vector<char*> HttpRequest::setupCGIEnvironment(const std::string& scriptPath)
{
    std::vector<std::string> envVars;

    envVars.push_back("REQUEST_METHOD=" + _method);
    envVars.push_back("SCRIPT_FILENAME=" + scriptPath);
    envVars.push_back("CONTENT_LENGTH=" + intToString(_body.size()));
    envVars.push_back("CONTENT_TYPE=" + _headers["Content-Type"]);
    envVars.push_back("GATEWAY_INTERFACE=CGI/1.1");
    envVars.push_back("SERVER_PROTOCOL=HTTP/1.1");
    envVars.push_back("REDIRECT_STATUS=200");

    std::vector<char*> env;
    for (size_t i = 0; i < envVars.size(); ++i)
    {
        env.push_back(strdup(envVars[i].c_str()));
    }
    env.push_back(NULL);

    return env;
}


void HttpRequest::createPipes(int outputPipe[2], int inputPipe[2])
{
    if (pipe(outputPipe) == -1 || pipe(inputPipe) == -1)
        throw std::runtime_error("Échec de la création des pipes");
}

std::string  HttpRequest::extractJsonValue(const std::string& json, const std::string& key)
{
    std::string keyPattern = "\"" + key + "\":\"";
    size_t keyPos = json.find(keyPattern);
    if (keyPos == std::string::npos)
        return "";

    size_t valueStart = keyPos + keyPattern.length();
    size_t valueEnd = json.find("\"", valueStart);
    if (valueEnd == std::string::npos)
        return "";

    return json.substr(valueStart, valueEnd - valueStart);
}

bool  HttpRequest::ensureUploadDirectoryExists()
{
    struct stat info;
    if (stat("var/www/upload", &info) != 0)
    {
        if (mkdir("var/www/upload", 0755) != 0)
            return false;
    }
    else if (!(info.st_mode & S_IFDIR))
        return false;
    return true;
}

std::string HttpRequest::uploadTxt(ServerConfig& config, std::string response)
{
    std::string fileName = extractJsonValue(this->_body, "fileName");
    std::string fileContent = extractJsonValue(this->_body, "fileContent");

    if (fileName.empty() || fileContent.empty())
        return findErrorPage(config, 400);

    if (!ensureUploadDirectoryExists())
        return findErrorPage(config, 500);

    std::string targetPath = "var/www/upload/" + fileName;
    std::ofstream outFile(targetPath.c_str(), std::ios::binary);
    if (!outFile.is_open())
        return findErrorPage(config, 500);

    outFile.write(fileContent.c_str(), fileContent.size());
    outFile.close();

    response = "HTTP/1.1 201 Created\r\n";
    response += "Content-Length: 0\r\n";
    response += "Content-Type: text/plain\r\n";
    response += "\r\n";
    return response;
}

std::string HttpRequest::uploadFile(ServerConfig& config, std::string response, std::string contentType)
{
    std::string boundary = "--" + contentType.substr(contentType.find("boundary=") + 9);
    size_t fileStartPos = _body.find("filename=\"");
    if (fileStartPos == std::string::npos)
        return findErrorPage(config, 400);

    fileStartPos += 10;
    size_t fileNameEndPos = _body.find("\"", fileStartPos);
    std::string fileName = _body.substr(fileStartPos, fileNameEndPos - fileStartPos);
    size_t contentStart = _body.find("\r\n\r\n", fileNameEndPos) + 4;
    size_t contentEnd = _body.find(boundary, contentStart) - 2;
    std::string fileContent = _body.substr(contentStart, contentEnd - contentStart);
    std::string targetPath = "var/www/upload/" + fileName;
    std::ofstream outFile(targetPath.c_str(), std::ios::binary);
    if (!outFile.is_open())
        return findErrorPage(config, 500);
    outFile.write(fileContent.c_str(), fileContent.size() - 46);
    outFile.close();

    response = "HTTP/1.1 201 Created\r\n";
    response += "Content-Length: 0\r\n";
    response += "Content-Type: text/plain\r\n";
    response += "\r\n";
    return response;
}

int HttpRequest::extractStatusCode(const std::string& response)
{
    size_t spacePos = response.find(' ');
    if (spacePos != std::string::npos) {
        size_t nextSpacePos = response.find(' ', spacePos + 1);
        if (nextSpacePos != std::string::npos) {
            std::string statusCodeStr = response.substr(spacePos + 1, nextSpacePos - spacePos - 1);
            return std::atoi(statusCodeStr.c_str());
        }
    }
    return 0;
}

std::string HttpRequest::generateDefaultErrorPage(int errorCode)
{
    std::ostringstream response;
    response << "<html><body><h1>Error " << errorCode << "</h1></body></html>";

    std::ostringstream httpResponse;
    httpResponse << "HTTP/1.1 " << errorCode << " Error\r\n";
    httpResponse << "Content-Type: text/html\r\n";
    httpResponse << "Content-Length: " << response.str().size() << "\r\n";
    httpResponse << "\r\n";
    httpResponse << response.str();

    return httpResponse.str();
}

std::string HttpRequest::getHeaderValue(const std::string& headerName) const
{
    std::map<std::string, std::string>::const_iterator it = _headers.find(headerName);
    if (it != _headers.end())
    {
        std::string value = it->second;
        if (!value.empty() && (value[value.size() - 1] == '\n' || value[value.size() - 1] == '\r')) {
            value.erase(value.size() - 1);
        }
        return value;
    }
    return "";
}
