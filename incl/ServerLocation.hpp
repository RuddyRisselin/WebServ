#ifndef SERVERLOCATION_HPP
#define SERVERLOCATION_HPP

#include <string>
#include <map>

class ServerLocation {
private:
    std::string _path;
    std::string _root;
    std::string _index;
    bool _getAllowed;
    bool _postAllowed;
    bool _deleteAllowed;

public:
    // Constructor
    ServerLocation(const std::string& path);

    // Getters and Setters
    const std::string& getPath() const;
    void setPath(const std::string& Path);

    void setRoot(const std::string& rootPath);
    const std::string& getRoot() const;
    
    void setIndex(const std::string& indexPage);
    const std::string& getIndex() const;
    
    void disableAllMethods();
    void allowGet();
    void allowPost();
    void allowDelete();
    bool isGetAllowed() const;
    bool isPostAllowed() const;
    bool isDeleteAllowed() const;

    void setAllowedMethods(const std::string& methodsLine);

    void display() const;
};

#endif
