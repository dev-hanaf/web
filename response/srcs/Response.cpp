#include "../include/Response.hpp"
#include <sstream>
#include <cstring>
#include <cctype>
#include <algorithm>

// Header validation constants
static const std::string HTTP_TOKEN_CHARS = 
    "!#$%&'*+-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`abcdefghijklmnopqrstuvwxyz|~";

// Validate header name (RFC 7230 Section 3.2.6)
bool Response::_isValidHeaderName(const std::string& name) const {
    if (name.empty()) return false;
    
    for (size_t i = 0; i < name.length(); ++i) {
        if (HTTP_TOKEN_CHARS.find(name[i]) == std::string::npos) {
            return false;
        }
    }
    return true;
}

// Validate header value (RFC 7230 Section 3.2)
bool Response::_isValidHeaderValue(const std::string& value) const {
    for (size_t i = 0; i < value.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        // Check for control characters except HTAB
        if (c < 32 && c != 9) return false;
        if (c == 127) return false;  // DEL character
    }
    return true;
}

// Sanitize header value
std::string Response::_sanitizeHeaderValue(const std::string& value) const {
    std::string sanitized;
    for (size_t i = 0; i < value.length(); ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        if (c >= 32 && c != 127) {
            sanitized += value[i];
        }
    }
    return sanitized;
}

Response::Response()
    : _statusCode(200), _statusMessage("OK"), _isBuilt(false), _isStreaming(false), _fileSize(0), _isChunked(false) {
    _addDefaultHeaders();
}

Response::Response(int statusCode)
    : _statusCode(statusCode), _isBuilt(false), _isStreaming(false), _fileSize(0), _isChunked(false) {
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
        // 2xx Success
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 206: return "Partial Content";
        
        // 3xx Redirection
        case 300: return "Multiple Choices";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        
        // 4xx Client Errors
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
        
        // 5xx Server Errors
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        
        default: return "Unknown";
    }
}

// Performance optimization fields
std::string _filePath;              // Path to file for direct serving
bool _isStreaming;                  // Should use streaming mode?
size_t _fileSize;                   // Size of file being served


void Response::setStatus(int code) {
    _statusCode = code;
    _statusMessage = _getStatusMessage(code);
    _isBuilt = false; // Invalidate cache
}

void Response::addHeader(const std::string& key, const std::string& value) {
    // Validate header name and value
    if (!_isValidHeaderName(key)) {
        std::cerr << "Warning: Invalid header name: " << key << std::endl;
        return;
    }
    
    std::string sanitizedValue = _sanitizeHeaderValue(value);
    if (!_isValidHeaderValue(sanitizedValue)) {
        std::cerr << "Warning: Invalid header value for " << key << std::endl;
        return;
    }
    
    _headers[key] = sanitizedValue;
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

    // Set current date if not already set (RFC 7231 Section 7.1.1.2)
    if (_headers.find("Date") == _headers.end() || _headers["Date"].empty()) {
        time_t now = time(NULL);
        char dateStr[100];
        strftime(dateStr, sizeof(dateStr), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
        _headers["Date"] = dateStr;
    }

    // Ensure Server header is present (RFC 7231 Section 7.4.2)
    if (_headers.find("Server") == _headers.end()) {
        _headers["Server"] = "WebServ/1.1";
    }

    // Handle Content-Length for responses with bodies (RFC 7230 Section 3.3.2)
    if (_statusCode != 204 && _statusCode != 304) {
        if (_isStreaming) {
            // For streaming responses, don't set Content-Length if using chunked encoding
            if (_headers.find("Transfer-Encoding") == _headers.end()) {
                // Set Content-Length for streaming if not chunked
                if (_fileSize > 0) {
                    std::ostringstream oss;
                    oss << _fileSize;
                    _headers["Content-Length"] = oss.str();
                }
            }
        } else if (!_body.empty()) {
            // For regular responses with body, ensure Content-Length is set
            if (_headers.find("Content-Length") == _headers.end()) {
                std::ostringstream oss;
                oss << _body.size();
                _headers["Content-Length"] = oss.str();
            }
        }
    }

    // Ensure Content-Type is set for responses with bodies (RFC 7231 Section 3.1.1.5)
    if (_statusCode != 204 && _statusCode != 304 && !_body.empty()) {
        if (_headers.find("Content-Type") == _headers.end()) {
            _headers["Content-Type"] = "text/plain"; // Default fallback
        }
    }

    // Headers (sorted for consistency)
    std::vector<std::pair<std::string, std::string> > sortedHeaders;
    for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); 
         it != _headers.end(); ++it) {
        sortedHeaders.push_back(*it);
    }
    
    // Sort headers for consistent output
    std::sort(sortedHeaders.begin(), sortedHeaders.end());
    
    for (std::vector<std::pair<std::string, std::string> >::const_iterator it = sortedHeaders.begin(); 
         it != sortedHeaders.end(); ++it) {
        res << it->first << ": " << it->second << "\r\n";
    }

    // End of headers
    res << "\r\n";

    // Body (only for non-204 responses)
    if (_statusCode != 204 && _statusCode != 304) {
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
    _filePath.clear();
    _isStreaming = false;
    _fileSize = 0;
    _isChunked = false;
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

void Response::setFilePath(const std::string& path) {
    _filePath = path;
    _isBuilt = false; // Invalidate cache
}

void Response::setStreaming(bool streaming) {
    _isStreaming = streaming;
    _isBuilt = false; // Invalidate cache
}

void Response::setFileSize(size_t size) {
    _fileSize = size;
    setContentLength(size);
    _isBuilt = false; // Invalidate cache
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

// CGI response handling
void Response::mergeCGIHeaders(const std::map<std::string, std::string>& cgiHeaders) {
    for (std::map<std::string, std::string>::const_iterator it = cgiHeaders.begin(); 
         it != cgiHeaders.end(); ++it) {
        const std::string& key = it->first;
        const std::string& value = it->second;
        
        // Handle special CGI headers
        if (key == "Status") {
            // Parse Status header (e.g., "200 OK")
            size_t spacePos = value.find(' ');
            if (spacePos != std::string::npos) {
                int statusCode = std::atoi(value.substr(0, spacePos).c_str());
                setStatus(statusCode);
            }
        } else if (key == "Content-Type") {
            // Ensure Content-Type is set
            addHeader("Content-Type", value);
        } else if (key == "Location") {
            // Handle redirects
            addHeader("Location", value);
        } else if (key == "Set-Cookie") {
            // Handle cookies (multiple Set-Cookie headers allowed)
            addHeader("Set-Cookie", value);
        } else {
            // Add other headers
            addHeader(key, value);
        }
    }
    
    _isBuilt = false; // Invalidate cache
}

void Response::setCGIStatus(int statusCode) {
    setStatus(statusCode);
}

// Chunked transfer encoding support
void Response::setChunkedEncoding(bool chunked) {
    _isChunked = chunked;
    if (chunked) {
        addHeader("Transfer-Encoding", "chunked");
        // Remove Content-Length for chunked responses
        _headers.erase("Content-Length");
    } else {
        _headers.erase("Transfer-Encoding");
    }
    _isBuilt = false;
}

std::string Response::buildHeadersOnly() const {
    std::ostringstream res;
    
    // Status line
    res << "HTTP/1.1 " << _statusCode << " " << _statusMessage << "\r\n";
    
    // Headers (sorted for consistency)
    std::vector<std::pair<std::string, std::string> > sortedHeaders;
    for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); 
         it != _headers.end(); ++it) {
        sortedHeaders.push_back(*it);
    }
    
    std::sort(sortedHeaders.begin(), sortedHeaders.end());
    
    for (std::vector<std::pair<std::string, std::string> >::const_iterator it = sortedHeaders.begin(); 
         it != sortedHeaders.end(); ++it) {
        res << it->first << ": " << it->second << "\r\n";
    }
    
    // End of headers
    res << "\r\n";
    
    return res.str();
}

std::string Response::buildChunk(const std::string& data) const {
    std::ostringstream chunk;
    chunk << std::hex << data.length() << "\r\n";
    chunk << data << "\r\n";
    return chunk.str();
}

std::string Response::buildFinalChunk() const {
    return "0\r\n\r\n";
}

