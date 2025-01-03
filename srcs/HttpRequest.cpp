/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: ymostows <ymostows@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/22 15:42:38 by ymostows          #+#    #+#             */
/*   Updated: 2024/11/22 15:42:38 by ymostows         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpRequest.hpp"

HttpRequest::HttpRequest(const std::string rawRequest) : _method(""), _path(""), _httpVersion(""), _body("")
{
    std::istringstream requestStream(rawRequest);
    std::string line;

    if (std::getline(requestStream, line))
    {
        std::istringstream lineStream(line);
        lineStream >> _method >> _path >> _httpVersion;
    }

    while (std::getline(requestStream, line) && line != "\r")
    {
        std::size_t delimiter = line.find(":");
        if (delimiter != std::string::npos)
        {
            std::string headerName = line.substr(0, delimiter);
            std::string headerValue = line.substr(delimiter + 2);
            _headers[headerName] = headerValue;
        }
    }

    if (_method == "POST")
    {
        std::map<std::string, std::string>::const_iterator it = _headers.find("Content-Length");
        if (it != _headers.end())
        {
            int contentLength = std::atoi(it->second.c_str());
            if (contentLength > 0)
            {
                std::vector<char> buffer(contentLength);
                requestStream.read(&buffer[0], contentLength);
                _body.assign(buffer.begin(), buffer.end());
            }
        }
    }
}

std::string HttpRequest::handleRequest(ServerConfig& config)
{
    const std::vector<ServerLocation>& locations = config.getLocations();
    if (_body.size() > config.getClientMaxBodySize())
        return findErrorPage(config, 413);
    for (std::vector<ServerLocation>::const_iterator it = locations.begin(); it != locations.end(); ++it)
    {
        if (_path == it->getPath())
        {
            if (_method == "GET" && !it->isGetAllowed())
                return findErrorPage(config, 405);
            if (_method == "POST" && !it->isPostAllowed())
                return findErrorPage(config, 405);
            if (_method == "DELETE" && !it->isDeleteAllowed())
                return findErrorPage(config, 405);
            break;
        }
    }
    if (_method == "GET")
        return handleGet(config);
    else if (_method == "POST")
        return handlePost(config);
    else if (_method == "DELETE")
        return handleDelete(config);
    else
        return findErrorPage(config, 400);
}

std::string HttpRequest::handleGet(ServerConfig& config)
{
    std::string fullPath = resolveFilePath(config);
    
    struct stat fileStat;
    if (stat(fullPath.c_str(), &fileStat) != 0)
        return findErrorPage(config, 404);
    if (S_ISDIR(fileStat.st_mode))
    {
        std::string indexPath = fullPath + "/index.html";
        if (access(indexPath.c_str(), F_OK) != 0)
            return findErrorPage(config, 403);
        fullPath = indexPath;
    }
    if (!isFileAccessible(fullPath))
        return findErrorPage(config, 404);
    if (fullPath.find(".py") != std::string::npos && fullPath.find("/var/www/upload/") == std::string::npos)
        return executeCGI(fullPath, config);
    std::string fileContent = readFile(fullPath);
    if (fileContent.empty() && S_ISREG(fileStat.st_mode))
    {
        std::string response = "HTTP/1.1 204 No Content\r\n";
        response += "Content-Length: 0\r\n";
        response += "Connection: close\r\n\r\n";
        return response;
    }
    if (fileContent.empty())
        return findErrorPage(config, 500);
    std::ostringstream oss;
    oss << fileContent.size();
    std::string response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Length: " + oss.str() + "\r\n";
    response += "Content-Type: " + getMimeType(fullPath) + "\r\n";
    if (_headers["Connection"] == "keep-alive")
        response += "Connection: keep-alive\r\n";
    else
        response += "Connection: close\r\n";
    response += "\r\n";
    response += fileContent;

    return response;
}

void HttpRequest::setupChildProcess(int outputPipe[2], int inputPipe[2], const std::string& scriptPath)
{
    close(outputPipe[0]);
    if (dup2(outputPipe[1], STDOUT_FILENO) == -1)
    {
        perror("Erreur de redirection de la sortie standard");
        exit(1);
    }
    close(outputPipe[1]);
    close(inputPipe[1]);
    if (dup2(inputPipe[0], STDIN_FILENO) == -1)
    {
        perror("Erreur de redirection de l'entrée standard");
        exit(1);
    }
    close(inputPipe[0]);
    std::vector<char*> env = setupCGIEnvironment(scriptPath);
    char* args[] = {(char*)"/usr/bin/python3", (char*)scriptPath.c_str(), NULL};
    execve("/usr/bin/python3", args, env.data());
    for (size_t i = 0; i < env.size(); ++i)
        free(env[i]);
    perror("Erreur d'exécution du script CGI");
    exit(1);
}

std::string HttpRequest::handleParentProcess(int outputPipe[2], int inputPipe[2], pid_t pid)
{
    close(outputPipe[1]);
    close(inputPipe[0]);
    ssize_t bytesWritten = write(inputPipe[1], _body.c_str(), _body.size());
    close(inputPipe[1]);
    if (bytesWritten == -1)
        throw std::runtime_error("Erreur lors de l'écriture dans le pipe d'entrée");

    std::string output;
    char buffer[1024];
    ssize_t bytesRead;
    while ((bytesRead = read(outputPipe[0], buffer, sizeof(buffer) - 1)) > 0)
    {
        buffer[bytesRead] = '\0';
        output += buffer;
    }
    close(outputPipe[0]);
    int status;
    if (waitpid(pid, &status, 0) == -1)
        throw std::runtime_error("Erreur lors de l'attente du processus CGI");
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
        throw std::runtime_error("Le script CGI a retourné une erreur");
    return constructCGIResponse(output);
}

std::string HttpRequest::executeCGI(const std::string& scriptPath, ServerConfig& config)
{
    (void)config;
    try
    {
        int outputPipe[2], inputPipe[2];
        createPipes(outputPipe, inputPipe);

        pid_t pid = fork();
        if (pid == 0)
        {
            setupChildProcess(outputPipe, inputPipe, scriptPath);
        }
        else if (pid > 0)
        {
            close(outputPipe[1]);
            close(inputPipe[0]);

            write(inputPipe[1], _body.c_str(), _body.size());
            close(inputPipe[1]);

            std::string output;
            char buffer[1024];
            ssize_t bytesRead;
            int status;
            int elapsedTime = 0;

            while (elapsedTime < 5)
            {
                pid_t result = waitpid(pid, &status, WNOHANG);
                if (result == pid)
                {
                    while ((bytesRead = read(outputPipe[0], buffer, sizeof(buffer) - 1)) > 0)
                    {
                        buffer[bytesRead] = '\0';
                        output += buffer;
                    }
                    close(outputPipe[0]);
                    
                    return constructCGIResponse(output);
                }
                else if (result == 0)
                {
                    usleep(500000);
                    elapsedTime++;
                }
                else
                    throw std::runtime_error("Erreur lors de l'attente du processus CGI");
            }
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            close(outputPipe[0]);
            return findErrorPage(config,504);
        }
        else
            throw std::runtime_error("Fork failed");
    }
    catch (const std::exception& e)
    {
        std::cerr << "Erreur CGI : " << e.what() << std::endl;
        return generateDefaultErrorPage(500);
    }
    return generateDefaultErrorPage(500);
}

std::string HttpRequest::handlePost(ServerConfig& config)
{
    std::string response;

    const std::vector<ServerLocation>& locations = config.getLocations();
    for (std::vector<ServerLocation>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
        if ("/post" == it->getPath() && _path != "/cgi-bin/auth.py")
        {
            if (!it->isPostAllowed())
                return findErrorPage(config, 405);
            break;
        }
    }
    std::map<std::string, std::string>::const_iterator it = this->_headers.find("Content-Length");
    if (it == this->_headers.end())
        return findErrorPage(config, 411);

    int contentLength;
    std::istringstream lengthStream(it->second);
    lengthStream >> contentLength;

    if (contentLength == 0)
        return findErrorPage(config, 400);
    if (this->_body.size() != static_cast<std::string::size_type>(contentLength))
        return findErrorPage(config, 400);
    std::map<std::string, std::string>::const_iterator contentTypeHeader = _headers.find("Content-Type");
    if (contentTypeHeader == _headers.end())
        return findErrorPage(config, 400);

    std::string contentType = contentTypeHeader->second;
    if (contentType.find("application/json") != std::string::npos)
        return (uploadTxt(config, response));
    else if (contentType.find("multipart/form-data") != std::string::npos)
        return (uploadFile(config, response, contentType));
    else if (contentType.find("application/x-www-form-urlencoded") != std::string::npos)
    {
        std::string scriptPath = _path;
        scriptPath = config.getRoot() + scriptPath.substr(1);
        return executeCGI(scriptPath, config);
    }
    else if (contentType.find("plain/text") != std::string::npos)
    {
        if (!ensureUploadDirectoryExists())
            return findErrorPage(config, 500);

        std::ostringstream fileNameStream;
        fileNameStream << "plain_text.txt";
        std::string fileName = fileNameStream.str();

        std::string targetRoot = resolveFilePath(config);
        std::string targetPath = targetRoot + "/" + fileName;

        std::ofstream outFile(targetPath.c_str(), std::ios::binary);
        if (!outFile.is_open())
            return findErrorPage(config, 500);

        outFile.write(this->_body.c_str(), this->_body.size());
        outFile.close();

        response = "HTTP/1.1 201 Created\r\n";
        response += "Content-Length: 0\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "\r\n";
        return response;
    }
    return findErrorPage(config, 415);
}

std::string HttpRequest::handleDelete(ServerConfig& config)
{
    const std::vector<ServerLocation>& locations = config.getLocations();
    for (std::vector<ServerLocation>::const_iterator it = locations.begin(); it != locations.end(); ++it) {
        if ("/delete" == it->getPath())
        {
            if (!it->isDeleteAllowed())
                return findErrorPage(config, 405);
            break;
        }
    }
    std::string resourcePath = "var/www/upload" + _path;

    struct stat fileStat;
    if (stat(resourcePath.c_str(), &fileStat) != 0)
        return findErrorPage(config, 404);

    if (access(resourcePath.c_str(), W_OK) != 0)
        return findErrorPage(config, 403);

    std::map<std::string, std::string>::const_iterator methodIt = _headers.find("Allow");
    if (methodIt != _headers.end() && methodIt->second.find("DELETE") == std::string::npos)
        return findErrorPage(config, 405);
    if (unlink(resourcePath.c_str()) != 0)
        return findErrorPage(config, 500);
    std::string response = "HTTP/1.1 204 Created\r\n";
        response += "Content-Length: 0\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "\r\n";
        return response;
}

std::string HttpRequest::findErrorPage(ServerConfig& config, int errorCode)
{
    std::string errorPagePath = config.getRoot() + config.getErrorPage(errorCode);
    if (errorPagePath.empty())
    {
        std::cerr << "Erreur : Aucune page d'erreur définie pour le code " << errorCode << std::endl;
        return generateDefaultErrorPage(errorCode);
    }
    std::string fullPath =  errorPagePath;
    struct stat fileStat;
    if (stat(fullPath.c_str(), &fileStat) != 0 || access(fullPath.c_str(), R_OK) != 0)
    {
        std::cerr << "Erreur : La page d'erreur " << fullPath << " est introuvable ou inaccessible" << std::endl;
        return generateDefaultErrorPage(errorCode);
    }
    std::ifstream errorFile(fullPath.c_str());
    if (!errorFile.is_open())
    {
        std::cerr << "Erreur : Impossible d'ouvrir la page d'erreur " << fullPath << std::endl;
        return generateDefaultErrorPage(errorCode);
    }

    std::string content;
    std::string line;
    while (std::getline(errorFile, line))
        content += line + "\n";
    errorFile.close();

    std::ostringstream response;
    response << "HTTP/1.1 " << errorCode << " Error\r\n";
    response << "Content-Type: text/html\r\n";
    response << "Content-Length: " << content.size() << "\r\n";
    response << "\r\n";
    response << content;

    return response.str();
}

std::string HttpRequest::getHttpVersion(void)
{
    return _httpVersion;
}

std::string HttpRequest::getMethod() const
{
    return _method;
}

std::string HttpRequest::getPath() const
{
    return _path;
}

HttpRequest::~HttpRequest()
{
}