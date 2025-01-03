#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <string.h>
#include <vector>
#include <limits>
#include <unistd.h>
#include <sys/wait.h>
#include "ServerConfig.hpp"
#include <sys/stat.h>
#include "ServerLocation.hpp"
#include <ctime>

class HttpRequest
{
private:
	std::string _method;
    std::string _path;
    std::string _httpVersion;
    std::string _body;
    std::map<std::string, std::string> _headers;
	
public:
	HttpRequest(const std::string rawRequest);
	~HttpRequest();

	std::string handleRequest(ServerConfig& config);
	std::string resolveFilePath(const ServerConfig& config);
	std::string readFile(const std::string& filePath);
	std::string handleGet(ServerConfig& config);
	std::string	handlePost(ServerConfig& config);
	std::string handleDownload(ServerConfig& config, std::string& response);
	std::string uploadTxt(ServerConfig& config, std::string response);
	std::string uploadFile(ServerConfig& config, std::string response, std::string contentType);
	std::string handleDelete(ServerConfig& config);
	std::string findErrorPage(ServerConfig& config, int errorCode);
	std::string getMimeType(const std::string& filePath);
	std::string getPath() const;
	std::string getMethod() const;
	std::string getHeaderValue(const std::string& headerName) const;
	std::string getHttpVersion(void);
	std::string handleParentProcess(int outputPipe[2], int inputPipe[2], pid_t pid);
	std::string constructCGIResponse(const std::string& output);
	std::string executeCGI(const std::string& scriptPath, ServerConfig& config);
	std::string generateDefaultErrorPage(int errorCode);
	std::string intToString(int value);
	std::string extractJsonValue(const std::string& json, const std::string& key);

	void createPipes(int outputPipe[2], int inputPipe[2]);
	void setupChildProcess(int outputPipe[2], int inputPipe[2], const std::string& scriptPath);
	std::vector<char*> setupCGIEnvironment(const std::string& scriptPath);

	bool ensureUploadDirectoryExists();
	bool isFileAccessible(const std::string& filePath);

	int extractStatusCode(const std::string& response);
};

#endif

