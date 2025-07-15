#include "../include/ResponseHandler.hpp"
#include "../../Connection.hpp"
#include "../../request/incs/Request.hpp"
#include "../../request/incs/RequestLine.hpp"
#include "../../request/incs/RequestHeaders.hpp"
#include "../../request/incs/RequestBody.hpp"
#include "../../conf/Server.hpp"
#include "../../conf/Location.hpp"
#include "../../conf/Root.hpp"
#include "../../conf/ClientMaxBodySize.hpp"
#include "../../conf/Index.hpp"
#include "../../conf/AutoIndex.hpp"
#include "../../conf/ErrorPage.hpp"
#include "../../conf/LimitExcept.hpp"
#include "../../conf/Index.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <cstring>
#include <algorithm>
#include <fcntl.h>
#include <sys/wait.h>
#include <cctype>
#include <ctime>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <sys/sendfile.h>
#include <iomanip> // Required for std::setw and std::setfill

// Helper function for toString
static std::string toString(int number) {
    std::stringstream ss;
    ss << number;
    return ss.str();
}

// Initialize static member
std::map<std::string, std::string> ResponseHandler::_mimeTypes;

void ResponseHandler::initialize() {
    // Initialize MIME types
    _mimeTypes[".html"] = "text/html";
    _mimeTypes[".htm"] = "text/html";
    _mimeTypes[".css"] = "text/css";
    _mimeTypes[".js"] = "application/javascript";
    _mimeTypes[".png"] = "image/png";
    _mimeTypes[".jpg"] = "image/jpeg";
    _mimeTypes[".jpeg"] = "image/jpeg";
    _mimeTypes[".gif"] = "image/gif";
    _mimeTypes[".ico"] = "image/x-icon";
    _mimeTypes[".txt"] = "text/plain";
    _mimeTypes[".pdf"] = "application/pdf";
    _mimeTypes[".py"] = "text/plain";
    _mimeTypes[".cgi"] = "text/plain";
}

Response ResponseHandler::handleRequest(Connection* conn) {
    if (!conn || !conn->req) {
        return createInternalErrorResponse();
    }

    Return* ret = conn->getReturnDirective();
    if (ret) {
        std::cout << "i am in return directive\n";
        // Only apply if the request matches the location's URI (for location context)
        const Location* loc = conn->getLocation();
        std::string reqUri = conn->req->getRequestLine().getUri();
        bool applyReturn = false;
        if (loc && loc->getDirective(RETURN) == ret) {
            std::string locUri = loc->getUri() ? std::string(loc->getUri()) : "";
            if (loc->isExactMatch()) {
                if (reqUri == locUri) applyReturn = true;
            } else {
                if (!locUri.empty() && reqUri.find(locUri) == 0) applyReturn = true;
            }
        } else {
            // Server-level return, apply if not in a location context
            applyReturn = true;
        }
        if (applyReturn) {
            unsigned int code = ret->getCode();
            char* url = ret->getUrl();
            std::string msg;
            if (code >= 300 && code < 400 && url && url[0]) {
                // Redirect
                Response response = Response::createRedirectResponse(code, std::string(url));
                return response;
            } else if (code >= 400 && code < 600) {
                if (url && url[0] == '/') {
                    std::string filePath = _getRootPath(conn) + url;
                    msg = loadFile(filePath);
                    if (msg.empty()) {
                        msg = "Error " + toString(code);
                    }
                } else if (url && url[0]) {
                    msg = std::string(url);
                } else {
                    msg = "Error " + toString(code);
                }
                return createErrorResponseWithMapping(conn, code, msg);
            } else {
                // Other codes: treat as error
                if (url && url[0]) {
                    msg = std::string(url);
                } else {
                    msg = "Error " + toString(code);
                }
                return createErrorResponseWithMapping(conn, code, msg);
            }
        }
    }

    const Request& request = *conn->req;
    std::string method = request.getRequestLine().getMethod();
    request.getRequestLine().getQueryParams();
    // Get allowed methods from configuration
    std::vector<std::string> allowedMethods = _getAllowedMethods(conn);
    // Check if method is allowed (move this before any other validation)
    std::cerr << "[DEBUG] Allowed methods for this resource: ";
    for (size_t i = 0; i < allowedMethods.size(); ++i) std::cerr << allowedMethods[i] << " ";
    std::cerr << "\n[DEBUG] Requested method: " << method << std::endl;
    if (request.getStatusCode() == REQUEST_TIMEOUT)
    {
        std::cout << "TIME OUT" << std::endl;
        return createErrorResponseWithMapping(conn, 408, "time out");
    }
    if (!_isAllowedMethod(method, allowedMethods)) {
        return createMethodNotAllowedResponse(allowedMethods);
    }
    // If method is not allowed, return 405 immediately, even if request status is not OK
    if (!_isAllowedMethod(method, allowedMethods)) {
        return createMethodNotAllowedResponse(allowedMethods);
    }
    // Check if request has any error status from parsing
    if (request.getStatusCode() != OK) {
        return createErrorResponseWithMapping(conn, request.getStatusCode(), "");
    }
    
    // Check body size for POST requests
    if (method == "POST") {
        if (!conn->checkMaxBodySize()) {
            return createErrorResponseWithMapping(conn, 413, "Request entity too large");
        }
    }
    
    // Handle different HTTP methods
    if (method == "GET") {
        return _handleGET(conn);
    } else if (method == "POST") {
        return _handlePOST(conn);
    } else if (method == "DELETE") {
        return _handleDELETE(conn);
    }
    return createErrorResponseWithMapping(conn, 501, "Method not implemented");
}

Response ResponseHandler::_handleGET(Connection* conn) {
    const Request& request = *conn->req;
    std::string uri = request.getRequestLine().getUri(); // This is already clean (no query string)
    std::string root = _getRootPath(conn);
    
    // Query string is already parsed and available via getQueryParams() for CGI
    std::string filePath = _buildFilePath(uri, root);

/*     // Check if it's a CGI script
    if (_isCGIScript(filePath)) {
        std::string cgiOutput = _executeCGI(filePath, request);
        if (!cgiOutput.empty()) {
            // Parse CGI output to separate headers and body
            std::map<std::string, std::string> cgiHeaders = _extractCGIHeaders(cgiOutput);
            std::string cgiBody = _parseCGIOutput(cgiOutput);
            
            Response response(200);
            response.setBody(cgiBody);
            
            // Apply CGI headers
            for (std::map<std::string, std::string>::const_iterator it = cgiHeaders.begin(); 
                 it != cgiHeaders.end(); ++it) {
                if (it->first != "Status") { // Skip Status header, we handle it separately
                    response.addHeader(it->first, it->second);
                }
            }
            
            // Check for custom status code
            if (cgiHeaders.find("Status") != cgiHeaders.end()) {
                std::string statusStr = cgiHeaders["Status"];
                size_t spacePos = statusStr.find(' ');
                if (spacePos != std::string::npos) {
                    int statusCode = atoi(statusStr.substr(0, spacePos).c_str());
                    response.setStatus(statusCode);
                }
            }
            
            return response;
        } else {
            return createErrorResponseWithMapping(conn, 500, "CGI execution failed");
        }
    } */

    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) != 0) {
        return createErrorResponseWithMapping(conn, 404, "The requested resource was not found");
    }

    if (S_ISDIR(fileStat.st_mode)) {
        // Always check for index files first
        std::vector<std::string> indexFiles = _getIndexFiles(conn);
        for (size_t i = 0; i < indexFiles.size(); ++i) {
            std::string indexPath = filePath + "/" + indexFiles[i];
            if (stat(indexPath.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode)) {
                std::string content = loadFile(indexPath);
                if (!content.empty()) {
                    Response response(200);
                    response.setBody(content);
                    response.setContentType(_getMimeType(indexPath));
                    return response;
                }
            }
        }
        // No index file found, check autoindex
        bool autoIndex = _getAutoIndex(conn);
        if (autoIndex) {
            std::string listing = _generateDirectoryListing(filePath, uri);
            Response response(200);
            response.setBody(listing);
            response.setContentType("text/html");
            return response;
        } else {
            std::cerr << "[DEBUG] Directory exists, autoindex off, returning 403 for: " << filePath << std::endl;
            return createErrorResponseWithMapping(conn, 403, "Directory listing not allowed");
        }
    }

    // Serve static file
    std::string mimeType = _getMimeType(filePath);
    
    // Use direct file serving for better performance
    if (_isFileServingSupported()) {
        return _serveFileDirectly(filePath, mimeType);
    } else {
        // Fallback to memory-based serving
        std::string content = loadFile(filePath);
        if (!content.empty()) {
            Response response(200);
            response.setBody(content);
            response.setContentType(mimeType);
            return response;
        } else {
            return createErrorResponseWithMapping(conn, 500, "Failed to read file");
        }
    }
}

Response ResponseHandler::_handlePOST(Connection* conn) {
    const Request& request = *conn->req;
    std::string uri = request.getRequestLine().getUri();
    
    // Handle file upload
    if (uri == "/upload" || true) {
        std::string uploadPath = _getUploadPath(conn);
        std::string result = _handleFileUpload(request, uploadPath);
        if (!result.empty()) {
            Response response(201);
            response.setBody(result);
            response.setContentType("text/html");
            return response;
        } else {
            return createInternalErrorResponse();
        }
    }
    
/*     // Handle CGI POST
    std::string root = _getRootPath(conn);
    std::string path = uri;
    // std::string path = _extractPath(uri);
    std::string filePath = _buildFilePath(path, root); */
    
/*     if (_isCGIScript(filePath)) {
        std::string cgiOutput = _executeCGI(filePath, request);
        if (!cgiOutput.empty()) {
            Response response(200);
            response.setBody(cgiOutput);
            response.setContentType("text/html");
            return response;
        } else {
            return createInternalErrorResponse();
        }
    } */
    
    return createNotFoundResponse(conn);
}

Response ResponseHandler::_handleDELETE(Connection* conn) {
    (void)conn;
    // const Request& request = *conn->req;
    // std::string uri = request.getRequestLine().getUri();
    // std::string root = _getRootPath(conn);
    // std::string path = uri;
    // std::string filePath = _buildFilePath(path, root);
    // if (_deleteFile(filePath)) {
    //     Response response(204);
    //     return response;
    // } else {
    //     return createNotFoundResponse();
    // } 
    return createNotFoundResponse(conn);
}

// Configuration extraction methods
std::string ResponseHandler::_getRootPath(Connection* conn) {
    if (!conn) return "www"; // Default fallback
    
    Root* root = conn->getRoot();
    if (root && root->getPath()) {
        return std::string(root->getPath());
    }
    return "www"; // Default
}

std::string ResponseHandler::_getUploadPath(Connection* conn) {
    if (!conn) return "uploads"; // Default fallback
    
    // For now, return a default upload path
    // In a real implementation, this would read from a configuration directive
    // like "upload_path" or similar
    return "uploads";
}

/* std::string ResponseHandler::_getCgiPath(Connection* conn) {
    (void)conn; // Suppress unused parameter warning
    // This would need to be implemented based on your configuration structure
    // For now, return a default
    return "cgi-bin";
} */

bool ResponseHandler::_getAutoIndex(Connection* conn) {
    if (!conn) return false; // Default fallback
    
    AutoIndex* autoindex = conn->getAutoIndex();
    if (autoindex) {
        return autoindex->getState();
    }
    return false; // Default
}

std::map<int, std::string> ResponseHandler::_getErrorPages(Connection* conn) {
    std::map<int, std::string> errorPages;
    if (!conn) return errorPages;
    
    // Check location context first
    const Location* location = conn->getLocation();
    if (location) {
        for (std::vector<IDirective*>::const_iterator dit = location->directives.begin(); dit != location->directives.end(); ++dit) {
            if ((*dit)->getType() == ERROR_PAGE) {
                ErrorPage* ep = static_cast<ErrorPage*>(*dit);
                if (ep && ep->getUri() && ep->getUri()[0])
                    errorPages[ep->getCode()] = std::string(ep->getUri());
            }
        }
    }
    // Then check server context (do not overwrite location mappings)
    if (conn->conServer) {
        for (std::vector<IDirective*>::const_iterator dit = conn->conServer->directives.begin(); dit != conn->conServer->directives.end(); ++dit) {
            if ((*dit)->getType() == ERROR_PAGE) {
                ErrorPage* ep = static_cast<ErrorPage*>(*dit);
                if (ep && ep->getUri() && ep->getUri()[0] && errorPages.find(ep->getCode()) == errorPages.end())
                    errorPages[ep->getCode()] = std::string(ep->getUri());
            }
        }
    }
    return errorPages;
}

std::vector<std::string> ResponseHandler::_getAllowedMethods(Connection* conn) {
    std::vector<std::string> methods;
    if (!conn) {
        // Default fallback - allow all methods
        methods.push_back("GET");
        methods.push_back("POST");
        methods.push_back("DELETE");
        return methods;
    }
    
    LimitExcept* limitExcept = conn->getLimitExcept();
    if (limitExcept) {
        char** allowedMethods = limitExcept->getMethods();
        if (allowedMethods) {
            for (int i = 0; allowedMethods[i] != NULL; ++i) {
                methods.push_back(std::string(allowedMethods[i]));
            }
        }
    }
    
    // If no methods specified, allow all
    if (methods.empty()) {
        methods.push_back("GET");
        methods.push_back("POST");
        methods.push_back("DELETE");
    }
    
    return methods;
}


std::vector<std::string> ResponseHandler::_getIndexFiles(Connection* conn) {
    std::vector<std::string> indexFiles;
    if (!conn) {
        // Default fallback
        indexFiles.push_back("index.html");
        indexFiles.push_back("index.htm");
        return indexFiles;
    }
    
    Index* index = conn->getIndex();
    if (index) {
        char** files = index->getFiles();
        if (files) {
            for (int i = 0; files[i] != NULL; ++i) {
                indexFiles.push_back(std::string(files[i]));
            }
        }
    }
    if (indexFiles.empty()) {
        indexFiles.push_back("index.html");
        indexFiles.push_back("index.htm");
    }
    return indexFiles;
}

// Helper methods
std::string ResponseHandler::_buildFilePath(const std::string& uri, const std::string& root) {
    if (root.empty()) {
        return uri;
    }
    
    std::string path = root;
    if (path[path.length() - 1] != '/') {
        path += "/";
    }
    
    // Remove leading slash from URI
    std::string cleanUri = uri;
    if (!cleanUri.empty() && cleanUri[0] == '/') {
        cleanUri = cleanUri.substr(1);
    }
    
    path += cleanUri;
    return path;
}

std::string ResponseHandler::_getMimeType(const std::string& path) {
    if (path.empty()) {
        return "text/plain";
    }
    
    size_t dotPos = path.find_last_of('.');
    if (dotPos != std::string::npos && dotPos < path.length() - 1) {
        std::string ext = path.substr(dotPos);
        std::map<std::string, std::string>::const_iterator it = _mimeTypes.find(ext);
        if (it != _mimeTypes.end()) {
            return it->second;
        }
    }
    return "text/plain";
}

std::string ResponseHandler::_getFileExtension(const std::string& path) {
    size_t dotPos = path.find_last_of('.');
    if (dotPos != std::string::npos && dotPos < path.length() - 1) {
        return path.substr(dotPos);
    }
    return "";
}

bool ResponseHandler::_isFileServingSupported() {
    // Check if sendfile is available on this system
    #ifdef __linux__
        return true;
    #else
        return false;
    #endif
}

Response ResponseHandler::_serveFileDirectly(const std::string& filePath, const std::string& mimeType) {
    std::cout << "[DEBUG] Serving file: " << filePath << std::endl;
    Response response(200);
    response.setContentType(mimeType);
    
    // Get file size for Content-Length header
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) == 0) {
        response.setFileSize(static_cast<size_t>(fileStat.st_size));
        
        // Use streaming for files larger than 1MB
        const size_t STREAMING_THRESHOLD = 1024 * 1024; // 1MB
        std::cout << "[DEBUG] File size: " << fileStat.st_size << std::endl;
        if (static_cast<size_t>(fileStat.st_size) > STREAMING_THRESHOLD) {
            response.setStreaming(true);
            response.setFilePath(filePath);
            std::cout << "[DEBUG] Using streaming for large file" << std::endl;
            // Don't load the entire file into memory
            return response;
        }
    } else {
        std::cout << "[ERROR] stat() failed for file: " << filePath << " - " << strerror(errno) << std::endl;
    }
    
    // For smaller files, load into memory
    std::string content = loadFile(filePath);
    if (!content.empty()) {
        response.setBody(content);
        std::cout << "[DEBUG] Loaded file into memory, size: " << content.size() << std::endl;
    } else {
        std::cout << "[ERROR] loadFile() returned empty for: " << filePath << std::endl;
    }
    
    return response;
}

/* bool ResponseHandler::_isCGIScript(const std::string& path) {
    if (path.empty()) return false;
    
    // Check for common CGI file extensions
    std::string lowerPath = path;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
    
    // Check for CGI extensions
    bool hasCGIExtension = (lowerPath.find(".py") != std::string::npos ||
                           lowerPath.find(".cgi") != std::string::npos ||
                           lowerPath.find(".php") != std::string::npos ||
                           lowerPath.find(".pl") != std::string::npos);
    
    // Check if file is executable or in cgi-bin directory
    bool isExecutable = false;
    struct stat fileStat;
    if (stat(path.c_str(), &fileStat) == 0) {
        isExecutable = (fileStat.st_mode & S_IXUSR) != 0;
    }
    
    bool inCGIBin = (lowerPath.find("cgi-bin") != std::string::npos);
    
    return hasCGIExtension && (isExecutable || inCGIBin);
} */

bool ResponseHandler::_isAllowedMethod(const std::string& method, const std::vector<std::string>& allowedMethods) {
    if (method.empty()) {
        return false;
    }
    return std::find(allowedMethods.begin(), allowedMethods.end(), method) != allowedMethods.end();
}

std::string ResponseHandler::_generateDirectoryListing(const std::string& path, const std::string& uri) {
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return "";
    }
    
    std::string html = "<html><head><title>Index of " + uri + "</title></head><body>";
    html += "<h1>Index of " + uri + "</h1><hr><ul>";
    html += "<h1> I am in generateDirectoryListing </h1>";

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (!entry) continue;
        
        std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        
        std::string fullPath = path + "/" + name;
        struct stat fileStat;
        if (stat(fullPath.c_str(), &fileStat) == 0) {
            std::string link = uri;
            if (link[link.length() - 1] != '/') {
                link += "/";
            }
            link += name;
            
            if (S_ISDIR(fileStat.st_mode)) {
                html += "<li><a href=\"" + link + "/\">" + name + "/</a></li>";
            } else {
                html += "<li><a href=\"" + link + "\">" + name + "</a></li>";
            }
        }
    }
    
    html += "</ul><hr></body></html>";
    closedir(dir);
    return html;
}

std::string ResponseHandler::_getErrorPage(int statusCode, const std::map<int, std::string>& errorPages) {
    std::map<int, std::string>::const_iterator it = errorPages.find(statusCode);
    if (it != errorPages.end()) {
        std::string content = loadFile(it->second);
        if (!content.empty()) {
            return content;
        }
    }
    
    // Default error pages
    switch (statusCode) {
        case 404:
            return "<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><p>The requested resource was not found.</p></body></html>";
        case 405:
            return "<html><head><title>405 Method Not Allowed</title></head><body><h1>405 Method Not Allowed</h1><p>The requested method is not allowed.</p></body></html>";
        case 413:
            return "<html><head><title>413 Request Entity Too Large</title></head><body><h1>413 Request Entity Too Large</h1><p>The request entity is too large.</p></body></html>";
        case 500:
            return "<html><head><title>500 Internal Server Error</title></head><body><h1>500 Internal Server Error</h1><p>An internal server error occurred.</p></body></html>";
        case 501:
            return "<html><head><title>501 Not Implemented</title></head><body><h1>501 Not Implemented</h1><p>The requested method is not implemented.</p></body></html>";
        default:
            return "<html><head><title>Error</title></head><body><h1>Error</h1><p>An error occurred.</p></body></html>";
    }
}

// CGI execution
/* std::string ResponseHandler::_executeCGI(const std::string& scriptPath, const Request& request) {
    // Create temporary file for CGI output
    std::string tempFile = "/tmp/cgi_output_" + toString(getpid()) + "_" + toString(time(NULL));
    
    // Set up environment variables
    std::map<std::string, std::string> env = _buildCGIEnvironment(request, scriptPath);
    
    // Create environment array
    std::vector<std::string> envArray;
    for (std::map<std::string, std::string>::const_iterator it = env.begin(); it != env.end(); ++it) {
        envArray.push_back(it->first + "=" + it->second);
    }
    
    // Create argv array
    std::vector<std::string> argv;
    argv.push_back("/usr/bin/python3");
    argv.push_back(scriptPath);
    
    // Convert to char* arrays
    std::vector<char*> envPtrs;
    for (size_t i = 0; i < envArray.size(); ++i) {
        envPtrs.push_back(const_cast<char*>(envArray[i].c_str()));
    }
    envPtrs.push_back(NULL);
    
    std::vector<char*> argvPtrs;
    for (size_t i = 0; i < argv.size(); ++i) {
        argvPtrs.push_back(const_cast<char*>(argv[i].c_str()));
    }
    argvPtrs.push_back(NULL);
    
    // Fork and execute
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        int outputFd = open(tempFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (outputFd != -1) {
            dup2(outputFd, STDOUT_FILENO);
            close(outputFd);
        }
        
        // Redirect stdin if there's POST data
        if (request.getRequestLine().getMethod() == "POST" && !request.getRequestBody().getRawData().empty()) {
            int inputFd = open("/tmp/cgi_input", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (inputFd != -1) {
                write(inputFd, request.getRequestBody().getRawData().c_str(), request.getRequestBody().getRawData().length());
                close(inputFd);
                
                inputFd = open("/tmp/cgi_input", O_RDONLY);
                if (inputFd != -1) {
                    dup2(inputFd, STDIN_FILENO);
                    close(inputFd);
                }
            }
        }
        
        execve(argvPtrs[0], &argvPtrs[0], &envPtrs[0]);
        exit(1);
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        
        // Read output
        std::string output = loadFile(tempFile);
        unlink(tempFile.c_str());
        unlink("/tmp/cgi_input");
        
        return output;
    }
    
    return "";
} */

/* std::map<std::string, std::string> ResponseHandler::_buildCGIEnvironment(const Request& request, const std::string& scriptPath) {
    std::map<std::string, std::string> env;
    
    // Required CGI variables
    env["REQUEST_METHOD"] = request.getRequestLine().getMethod();
    env["REQUEST_URI"] = request.getRequestLine().getUri();
    // Reconstruct query string from getQueryParams()
    const std::map<std::string, std::string>& params = request.getRequestLine().getQueryParams();
    std::string queryString;
    for (std::map<std::string, std::string>::const_iterator it = params.begin(); it != params.end(); ++it) {
        if (it != params.begin()) queryString += "&";
        queryString += it->first + "=" + it->second;
    }
    env["QUERY_STRING"] = queryString;
    env["SERVER_PROTOCOL"] = request.getRequestLine().getVersion();
    
    // Server information
    env["SERVER_NAME"] = "localhost"; // Should come from configuration
    env["SERVER_PORT"] = "8080"; // Should come from configuration
    env["SERVER_SOFTWARE"] = "WebServ/1.1";
    
    // Script information
    env["SCRIPT_NAME"] = scriptPath;
    env["SCRIPT_FILENAME"] = scriptPath;
    env["PATH_INFO"] = request.getRequestLine().getUri();
    env["PATH_TRANSLATED"] = scriptPath;
    
    // Request headers (prefixed with HTTP_)
    env["HTTP_HOST"] = request.getRequestHeaders().getHeaderValue("host");
    env["HTTP_USER_AGENT"] = request.getRequestHeaders().getHeaderValue("user-agent");
    env["HTTP_ACCEPT"] = request.getRequestHeaders().getHeaderValue("accept");
    env["HTTP_ACCEPT_LANGUAGE"] = request.getRequestHeaders().getHeaderValue("accept-language");
    env["HTTP_ACCEPT_ENCODING"] = request.getRequestHeaders().getHeaderValue("accept-encoding");
    env["HTTP_CONNECTION"] = request.getRequestHeaders().getHeaderValue("connection");
    env["HTTP_REFERER"] = request.getRequestHeaders().getHeaderValue("referer");
    
    // Content information
    env["CONTENT_TYPE"] = request.getRequestHeaders().getHeaderValue("content-type");
    env["CONTENT_LENGTH"] = toString(request.getRequestBody().getContentLength());
    
    // Remote information
    env["REMOTE_ADDR"] = "127.0.0.1"; // Should come from connection
    env["REMOTE_HOST"] = "localhost";
    env["REMOTE_PORT"] = "0"; // Should come from connection
    
    // Document information
    env["DOCUMENT_ROOT"] = "www"; // Should come from configuration
    env["DOCUMENT_URI"] = request.getRequestLine().getUri();
    
    return env;
} */

/* std::string ResponseHandler::_parseCGIOutput(const std::string& cgiOutput) {
    // CGI output format: headers\r\n\r\nbody
    size_t headerEnd = cgiOutput.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        // Try \n\n as fallback
        headerEnd = cgiOutput.find("\n\n");
        if (headerEnd == std::string::npos) {
            // No headers found, treat entire output as body
            return cgiOutput;
        }
        headerEnd += 2;
    } else {
        headerEnd += 4;
    }
    
    // Extract headers and body
    std::string headers = cgiOutput.substr(0, headerEnd);
    std::string body = cgiOutput.substr(headerEnd);
    
    // Parse CGI headers
    std::map<std::string, std::string> cgiHeaders;
    std::istringstream headerStream(headers);
    std::string line;
    
    while (std::getline(headerStream, line) && !line.empty() && line != "\r") {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t\r") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r") + 1);
            
            cgiHeaders[key] = value;
        }
    }
    
    // Return the body (headers will be processed separately)
    return body;
} */

/* std::map<std::string, std::string> ResponseHandler::_extractCGIHeaders(const std::string& cgiOutput) {
    std::map<std::string, std::string> cgiHeaders;
    
    size_t headerEnd = cgiOutput.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        headerEnd = cgiOutput.find("\n\n");
        if (headerEnd == std::string::npos) {
            return cgiHeaders;
        }
        headerEnd += 2;
    } else {
        headerEnd += 4;
    }
    
    std::string headers = cgiOutput.substr(0, headerEnd);
    std::istringstream headerStream(headers);
    std::string line;
    
    while (std::getline(headerStream, line) && !line.empty() && line != "\r") {
        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string key = line.substr(0, colonPos);
            std::string value = line.substr(colonPos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t\r") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t\r") + 1);
            
            cgiHeaders[key] = value;
        }
    }
    
    return cgiHeaders;
} */

// File operations
/* bool ResponseHandler::_deleteFile(const std::string& filePath) {
    return (unlink(filePath.c_str()) == 0);
}



// Utility methods
std::string ResponseHandler::_urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::istringstream iss(str.substr(i + 1, 2));
            iss >> std::hex >> value;
            result += static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string ResponseHandler::_urlEncode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (isalnum(str[i]) || str[i] == '-' || str[i] == '_' || str[i] == '.' || str[i] == '~') {
            result += str[i];
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", static_cast<unsigned char>(str[i]));
            result += hex;
        }
    }
    return result;
} */

std::string ResponseHandler::_urlDecode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value;
            std::istringstream iss(str.substr(i + 1, 2));
            iss >> std::hex >> value;
            result += static_cast<char>(value);
            i += 2;
        } else if (str[i] == '+') {
            result += ' ';
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string ResponseHandler::_urlEncode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        char c = str[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result += c;
        } else if (c == ' ') {
            result += '+';
        } else {
            std::ostringstream oss;
            oss << '%' << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(c));
            result += oss.str();
        }
    }
    return result;
}

std::string ResponseHandler::_handleFileUpload(const Request& request, const std::string& uploadPath) {
    std::string contentType = request.getRequestHeaders().getHeaderValue("content-type");
    std::string body = request.getRequestBody().getRawData();
    std::cout << "[DEBUG] Upload handler: content-type=" << contentType << ", body size=" << body.size() << std::endl;
    
    if (contentType.find("multipart/form-data") != std::string::npos) {
        // Parse multipart data
        size_t boundaryPos = contentType.find("boundary=");
        if (boundaryPos != std::string::npos) {
            std::string boundary = "--" + contentType.substr(boundaryPos + 9);
            size_t pos = body.find(boundary);
            if (pos != std::string::npos) {
                pos = body.find("\r\n\r\n", pos);
                if (pos != std::string::npos) {
                    pos += 4;
                    size_t endPos = body.find(boundary, pos);
                    if (endPos != std::string::npos) {
                        std::string fileData = body.substr(pos, endPos - pos - 2);
                        std::string filename = "upload_" + toString(time(NULL)) + ".txt";
                        std::string filepath = uploadPath + "/" + filename;
                        std::cout << "[DEBUG] Writing upload to: " << filepath << ", size=" << fileData.size() << std::endl;
                        if (mkdir(uploadPath.c_str(), 0755) != 0 && errno != EEXIST) {
                            std::cout << "[ERROR] Failed to create upload dir: " << strerror(errno) << std::endl;
                            return "";
                        }
                        std::ofstream file(filepath.c_str());
                        if (file.is_open()) {
                            file.write(fileData.c_str(), fileData.length());
                            file.close();
                            std::cout << "[DEBUG] Upload write success" << std::endl;
                            return "<html><head><title>File Uploaded</title></head><body><h1>File Uploaded Successfully</h1><p>File saved as: " + filename + "</p></body></html>";
                        } else {
                            std::cout << "[ERROR] Failed to open file for writing: " << filepath << std::endl;
                        }
                    } else {
                        std::cout << "[ERROR] End boundary not found in body" << std::endl;
                    }
                } else {
                    std::cout << "[ERROR] Multipart header/body separator not found" << std::endl;
                }
            } else {
                std::cout << "[ERROR] Boundary not found in body" << std::endl;
            }
        } else {
            std::cout << "[ERROR] Boundary not found in content-type" << std::endl;
        }
    } else {
        std::cout << "[ERROR] Not multipart/form-data: " << contentType << std::endl;
    }
    return "";
}
// Error response helpers
Response ResponseHandler::createErrorResponse(int statusCode, const std::string& message) {
    // This is now a fallback, prefer createErrorResponseWithMapping
    return Response::createErrorResponse(statusCode, message);
}

Response ResponseHandler::createMethodNotAllowedResponse(const std::vector<std::string>& allowedMethods) {
    Response response(405);
    std::string methods;
    for (size_t i = 0; i < allowedMethods.size(); ++i) {
        if (i > 0) methods += ", ";
        methods += allowedMethods[i];
    }
    response.setAllow(methods);
    response.setBody("<html><head><title>405 Method Not Allowed</title></head><body><h1>405 Method Not Allowed</h1><p>The requested method is not allowed.</p></body></html>");
    response.setContentType("text/html");
    std::cerr << "[DEBUG] Returning 405 Method Not Allowed:\n" << response.build() << std::endl;
    return response;
}

Response ResponseHandler::createNotFoundResponse(Connection* conn) {
    // Use error_page mapping for 404
    return createErrorResponseWithMapping(conn, 404, "The requested resource was not found");
}

Response ResponseHandler::createForbiddenResponse() {
    // Use error_page mapping for 403
    return createErrorResponseWithMapping(NULL, 403, "Access forbidden");
}

Response ResponseHandler::createInternalErrorResponse() {
    // Use error_page mapping for 500
    return createErrorResponseWithMapping(NULL, 500, "Internal server error");
}

// Helper for error_page mapping
Response ResponseHandler::createErrorResponseWithMapping(Connection* conn, int statusCode, const std::string& message) {
    // Try error_page mapping (location > server)
    if (conn) {
        ErrorPage* ep = conn->getErrorPageForCode(statusCode);
        if (ep && ep->getUri() && ep->getUri()[0]) {
            // Build file path using root (location > server)
            std::string root = _getRootPath(conn);
            std::string errorPath = root;
            if (errorPath[errorPath.length() - 1] != '/') errorPath += "/";
            std::string uriStr(ep->getUri());
            if (uriStr[0] == '/') uriStr = uriStr.substr(1);
            errorPath += uriStr;
            struct stat fileStat;
            if (stat(errorPath.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode)) {
                std::string content = loadFile(errorPath);
                if (!content.empty()) {
                    Response response(statusCode);
                    response.setBody(content);
                    response.setContentType(_getMimeType(errorPath));
                    std::cerr << "[DEBUG] Returning custom error page for " << statusCode << ":\n" << response.build() << std::endl;
                    return response;
                }
            }
        }
    }
    // Fallback to built-in error for 403
    if (statusCode == 403) {
        Response response = Response::createErrorResponse(403, message);
        std::cerr << "[DEBUG] Returning built-in 403 error:\n" << response.build() << std::endl;
        return response;
    }
    // Fallback to built-in error for others
    Response response = Response::createErrorResponse(statusCode, message);
    std::cerr << "[DEBUG] Returning built-in error " << statusCode << ":\n" << response.build() << std::endl;
    return response;
} 