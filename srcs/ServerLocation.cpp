#include "ServerLocation.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

ServerLocation::ServerLocation(const std::string& path) : _path(path), _root(""), _index(""), _getAllowed(true), _postAllowed(true), _deleteAllowed(true)
{
    if (path.empty())
        throw std::runtime_error("Error: Path cannot be empty in location block");
}

const std::string& ServerLocation::getPath() const
{
    return _path;
}

void ServerLocation::setPath(const std::string& Path)
{
    this->_path = Path;
}

void ServerLocation::setRoot(const std::string& rootPath)
{
    this->_root = rootPath;
}

const std::string& ServerLocation::getRoot() const
{
    return _root;
}

void ServerLocation::setIndex(const std::string& indexPage)
{
    this->_index = indexPage;
}

const std::string& ServerLocation::getIndex() const
{
    return _index;
}

void ServerLocation::disableAllMethods()
{
    _getAllowed = false;
    _postAllowed = false;
    _deleteAllowed = false;
}

void ServerLocation::allowGet()
{
    _getAllowed = true;
}

void ServerLocation::allowPost()
{
    _postAllowed = true;
}

void ServerLocation::allowDelete()
{
    _deleteAllowed = true;
}

bool ServerLocation::isGetAllowed() const
{
    return _getAllowed;
}

bool ServerLocation::isPostAllowed() const
{
    return _postAllowed;
}

bool ServerLocation::isDeleteAllowed() const
{
    return _deleteAllowed;
}

void ServerLocation::setAllowedMethods(const std::string& methodsLine)
{
    _getAllowed = false;
    _postAllowed = false;
    _deleteAllowed = false;

    std::istringstream iss(methodsLine);
    std::string method;

    while (iss >> method)
    {
        std::transform(method.begin(), method.end(), method.begin(), ::toupper);
        if (method == "GET")
            _getAllowed = true;
        else if (method == "POST")
            _postAllowed = true;
        else if (method == "DELETE")
            _deleteAllowed = true;
    }
}

void ServerLocation::display() const
{
    std::cout << "----------location----------\n";
    std::cout << "Location Path: " << _path << std::endl;
    
    std::cout << "root : " << _root << std::endl;

    std::cout << "index : " << _index << std::endl;

    std::cout << "Allowed Methods:\n";
    std::cout << "  GET: " << (_getAllowed ? "Yes" : "No") << std::endl;
    std::cout << "  POST: " << (_postAllowed ? "Yes" : "No") << std::endl;
    std::cout << "  DELETE: " << (_deleteAllowed ? "Yes" : "No") << std::endl;

    std::cout << "-----------------------\n" << std::endl;
    
}
