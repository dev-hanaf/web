#include "../include/Response.hpp"
#include <sstream>
#include <cstring>

Response::Response()
    : _statusCode(200), _statusMessage("OK"), _isBuilt(false) {
    _addDefaultHeaders();
}

Response::Response(int statusCode)
    : _statusCode(statusCode), _isBuilt(false) {
    _statusMessage = _getStatusMessage(statusCode);
    _addDefaultHeaders();
}

Response::~Response() {}

void Response::_addDefaultHeaders() {
    _headers["Server"] = "WebServ/1.0";
    _headers["Date"] = ""; // Will be set in build()
}

std::string Response::_getStatusMessage(int code) const {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 411: return "Length Required";
        case 413: return "Request Entity Too Large";
        case 414: return "URI Too Long";
        case 415: return "Unsupported Media Type";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 505: return "HTTP Version Not Supported";
        default: return "Unknown";
    }
}

void Response::setStatus(int code) {
    _statusCode = code;
    _statusMessage = _getStatusMessage(code);
    _isBuilt = false; // Invalidate cache
}

void Response::addHeader(const std::string& key, const std::string& value) {
    _headers[key] = value;
    _isBuilt = false; // Invalidate cache
}

void Response::setBody(const std::string& bodyContent) {
    _body = bodyContent;
    setContentLength(_body.size());
    _isBuilt = false; // Invalidate cache
}

void Response::setBody(const std::vector<char>& bodyContent) {
    _body.assign(bodyContent.begin(), bodyContent.end());
    setContentLength(_body.size());
    _isBuilt = false; // Invalidate cache
}

std::string Response::build() {
    if (_isBuilt && !_cachedResponse.empty()) {
        return _cachedResponse;
    }

    std::ostringstream res;

    // Status line
    res << "HTTP/1.1 " << _statusCode << " " << _statusMessage << "\r\n";

    // Set current date if not already set
    if (_headers.find("Date") == _headers.end() || _headers["Date"].empty()) {
        time_t now = time(NULL);
        char dateStr[100];
        strftime(dateStr, sizeof(dateStr), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
        _headers["Date"] = dateStr;
    }

    // Headers
    for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); 
         it != _headers.end(); ++it) {
        res << it->first << ": " << it->second << "\r\n";
    }

    // End of headers
    res << "\r\n";

    // Body (only for non-204 responses)
    if (_statusCode != 204) {
        res << _body;
    }

    _cachedResponse = res.str();
    _isBuilt = true;
    return _cachedResponse;
}

void Response::clear() {
    _statusCode = 200;
    _statusMessage = "OK";
    _headers.clear();
    _body.clear();
    _isBuilt = false;
    _cachedResponse.clear();
    _addDefaultHeaders();
}

bool Response::isEmpty() const {
    return _body.empty() && _statusCode == 200;
}

size_t Response::getContentLength() const {
    return _body.size();
}

void Response::setContentType(const std::string& type) {
    addHeader("Content-Type", type);
}

void Response::setContentLength(size_t length) {
    std::ostringstream oss;
    oss << length;
    addHeader("Content-Length", oss.str());
}

void Response::setConnection(const std::string& connection) {
    addHeader("Connection", connection);
}

void Response::setLocation(const std::string& location) {
    addHeader("Location", location);
}

void Response::setAllow(const std::string& methods) {
    addHeader("Allow", methods);
}

Response Response::createErrorResponse(int statusCode, const std::string& message) {
    Response response(statusCode);
    
    std::string errorBody = "<html><head><title>" + 
                           response.getStatusMessage() + "</title></head><body>";
    errorBody += "<h1>" + response.getStatusMessage() + "</h1>";
    if (!message.empty()) {
        errorBody += "<p>" + message + "</p>";
    }
    errorBody += "</body></html>";
    
    response.setBody(errorBody);
    response.setContentType("text/html");
    return response;
}

Response Response::createRedirectResponse(int statusCode, const std::string& location) {
    Response response(statusCode);
    response.setLocation(location);
    response.setContentType("text/html");
    
    std::string body = "<html><head><title>Redirect</title></head><body>";
    body += "<h1>Redirect</h1><p>The document has moved <a href=\"" + location + "\">here</a>.</p>";
    body += "</body></html>";
    
    response.setBody(body);
    return response;
}

