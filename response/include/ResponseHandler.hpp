#pragma once

#include <string>
#include <map>
#include <vector>
#include "Response.hpp"
#include "utils.hpp"

// Forward declarations
class Request;
class Server;
class Location;
class Connection;

class ResponseHandler {
private:
    // MIME types mapping
    static std::map<std::string, std::string> _mimeTypes;
    
    // Helper methods for configuration extraction
    static std::string _getRootPath( Connection* conn);
    static std::string _getUploadPath( Connection* conn);
    /* static std::string _getCgiPath( Connection* conn); */
    static bool _getAutoIndex( Connection* conn);
    static std::map<int, std::string> _getErrorPages( Connection* conn);
    static std::vector<std::string> _getAllowedMethods( Connection* conn);
    static std::vector<std::string> _getIndexFiles( Connection* conn);
    
    // Helper methods for request processing
    static std::string _buildFilePath(const std::string& uri, const std::string& root);
    static std::string _getMimeType(const std::string& path);
    static bool _isCGIScript(const std::string& path);    
    static bool _isAllowedMethod(const std::string& method, const std::vector<std::string>& allowedMethods);
    static std::string _generateDirectoryListing(const std::string& path, const std::string& uri);
    static std::string _getErrorPage(int statusCode, const std::map<int, std::string>& errorPages);
    
    // HTTP method handlers
    static Response _handleGET( Connection* conn);
    static Response _handlePOST( Connection* conn);
    static Response _handleDELETE( Connection* conn);
    
    // CGI execution and parsing
    static std::string _executeCGI(const std::string& scriptPath, const Request& request);
    static std::map<std::string, std::string> _buildCGIEnvironment(const Request& request, const std::string& scriptPath);
    static std::string _parseCGIOutput(const std::string& cgiOutput);
    static std::map<std::string, std::string> _extractCGIHeaders(const std::string& cgiOutput);
    
    // File operations
static std::string _handleFileUpload(const Request& request, const std::string& uploadPath);
    /*     static bool _deleteFile(const std::string& filePath);
     */
    
    // Direct file serving
    static Response _serveFileDirectly(const std::string& filePath, const std::string& mimeType);
    static bool _isFileServingSupported();
    static std::string _getFileExtension(const std::string& path);
    
    // Utility methods
    static std::string _urlDecode(const std::string& str);
    static std::string _urlEncode(const std::string& str);
    
public:
    // Main entry point - handles the entire request processing
    static Response handleRequest( Connection* conn);
    
    // Error response helpers
    static Response createErrorResponse(int statusCode, const std::string& message = "");
    static Response createMethodNotAllowedResponse(const std::vector<std::string>& allowedMethods);
    static Response createNotFoundResponse(Connection* conn);
    static Response createForbiddenResponse();
    static Response createInternalErrorResponse();
    static Response createErrorResponseWithMapping(Connection* conn, int statusCode, const std::string& message = "");
    
    // Initialize static members
    static void initialize();
}; 