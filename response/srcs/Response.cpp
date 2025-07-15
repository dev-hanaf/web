#include "../include/Response.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>
#include <ctime>

Response::Response() : _statusCode(200), _statusMessage("OK"), _fileSize(0), _isBuilt(false) {
    _addDefaultHeaders();
}

Response::Response(int statusCode) : _statusCode(statusCode), _statusMessage(_getStatusMessage(statusCode)), _fileSize(0), _isBuilt(false) {
    _addDefaultHeaders();
}

Response::~Response() {}

void Response::_addDefaultHeaders() {
    _headers["Server"] = "WebServ/1.0";
    _headers["Date"] = "";
}

std::string Response::_getStatusMessage(int code) const {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 206: return "Partial Content";
        case 300: return "Multiple Choices";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 406: return "Not Acceptable";
        case 408: return "Request Timeout";
        case 409: return "Conflict";
        case 411: return "Length Required";
        case 413: return "Request Entity Too Large";
        case 414: return "URI Too Long";
        case 415: return "Unsupported Media Type";
        case 422: return "Unprocessable Entity";
        case 429: return "Too Many Requests";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        default: return "Unknown";
    }
}

void Response::setStatus(int code) {
    _statusCode = code;
    _statusMessage = _getStatusMessage(code);
    _isBuilt = false;
}

void Response::addHeader(const std::string& key, const std::string& value) {
    _headers[key] = value;
    _isBuilt = false;
}

void Response::setFileBody(const std::string& filePath) {
    setFilePath(filePath);
    struct stat fileStat;
    if (stat(filePath.c_str(), &fileStat) == 0) {
        setFileSize(static_cast<size_t>(fileStat.st_size));
    }
    _isBuilt = false;
}

std::string Response::build() {
    if (_isBuilt && !_cachedResponse.empty()) return _cachedResponse;
    std::ostringstream res;
    res << "HTTP/1.1 " << _statusCode << " " << _statusMessage << "\r\n";
    if (_headers.find("Date") == _headers.end() || _headers["Date"].empty()) {
        time_t now = time(NULL);
        char dateStr[100];
        strftime(dateStr, sizeof(dateStr), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
        _headers["Date"] = dateStr;
    }
    if (_headers.find("Server") == _headers.end()) {
        _headers["Server"] = "WebServ/1.1";
    }
    if (_statusCode != 204 && _statusCode != 304) {
        if (_headers.find("Content-Length") == _headers.end()) {
            std::ostringstream oss;
            oss << _fileSize;
            _headers["Content-Length"] = oss.str();
        }
    }
    if (_statusCode != 204 && _statusCode != 304 && !_filePath.empty()) {
        if (_headers.find("Content-Type") == _headers.end()) {
            _headers["Content-Type"] = "text/plain";
        }
    }
    std::vector<std::pair<std::string, std::string> > sortedHeaders(_headers.begin(), _headers.end());
    std::sort(sortedHeaders.begin(), sortedHeaders.end());
    for (size_t i = 0; i < sortedHeaders.size(); ++i) {
        res << sortedHeaders[i].first << ": " << sortedHeaders[i].second << "\r\n";
    }
    res << "\r\n";
    _cachedResponse = res.str();
    _isBuilt = true;
    return _cachedResponse;
}

void Response::clear() {
    _statusCode = 200;
    _statusMessage = "OK";
    _headers.clear();
    _filePath.clear();
    _isBuilt = false;
    _cachedResponse.clear();
    _addDefaultHeaders();
}

void Response::setContentType(const std::string& type) {
    addHeader("Content-Type", type);
}

void Response::setFilePath(const std::string& path) {
    _filePath = path;
    _isBuilt = false;
}

void Response::setFileSize(size_t size) {
    _fileSize = size;
    _isBuilt = false;
}

Response Response::createRedirectResponse(int statusCode, const std::string& location) {
    Response response(statusCode);
    response.addHeader("Location", location);
    response.setContentType("text/html");
    
    std::string tempPath = "/tmp/webserv_redirect.html";
    std::ofstream tempFile(tempPath.c_str());
    tempFile << "<html><head><title>Redirect</title></head><body>"
             << "<h1>Redirect</h1><p>The document has moved <a href=\"" 
             << location << "\">here</a>.</p></body></html>";
    tempFile.close();
    
    response.setFileBody(tempPath);
    return response;
}