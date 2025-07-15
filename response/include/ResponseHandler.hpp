#pragma once

#include <string>
#include <map>
#include <vector>
#include "Response.hpp"
#include "utils.hpp"

class Request;
class Server;
class Location;
class Connection;

class ResponseHandler {
public:
    static std::string getRootPath(Connection* conn);
    static std::string getUploadPath(Connection* conn);
    static bool getAutoIndex(Connection* conn);
    static std::vector<std::string> getAllowedMethods(Connection* conn);
    static std::vector<std::string> getIndexFiles(Connection* conn);
    static std::map<int, std::string> getErrorPages(Connection* conn);
    static std::string getErrorPage(int code, const std::map<int, std::string>& e);
    static std::string buildFilePath(const std::string& uri, const std::string& root);
    static bool isAllowedMethod(const std::string& method, const std::vector<std::string>& allowedMethods);
    static std::string generateDirectoryListing(const std::string& path, const std::string& uri);
    static std::string getMimeType(const std::string& path);
    static Response handleRequest(Connection* conn);
    static void initialize();
}; 