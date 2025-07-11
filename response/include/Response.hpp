#pragma once

#include <iostream>
#include <map>
#include <string>
#include <vector>



// Foreground colors
#define RESET       "\033[0m"
#define BLACK       "\033[30m"
#define RED         "\033[31m"
#define GREEN       "\033[32m"
#define YELLOW      "\033[33m"
#define BLUE        "\033[34m"
#define MAGENTA     "\033[35m"
#define CYAN        "\033[36m"
#define WHITE       "\033[37m"

// Bright foreground colors
#define BBLACK      "\033[90m"
#define BRED        "\033[91m"
#define BGREEN      "\033[92m"
#define BYELLOW     "\033[93m"
#define BBLUE       "\033[94m"
#define BMAGENTA    "\033[95m"
#define BCYAN       "\033[96m"
#define BWHITE      "\033[97m"

// Background colors (optional)
#define BG_BLACK    "\033[40m"
#define BG_RED      "\033[41m"
#define BG_GREEN    "\033[42m"
#define BG_YELLOW   "\033[43m"
#define BG_BLUE     "\033[44m"
#define BG_MAGENTA  "\033[45m"
#define BG_CYAN     "\033[46m"
#define BG_WHITE    "\033[47m"

class Response
{
    private:
        int _statusCode;
        std::string _statusMessage;
        std::map<std::string, std::string> _headers;
        std::string _body;
        bool _isBuilt;
        std::string _cachedResponse;
        
        std::string _filePath;  // For direct file serving
        bool _isStreaming;      // For large file streaming
        size_t _fileSize;       // File size for Content-Length
        bool _isChunked;        // For chunked transfer encoding

        // Helper methods
        std::string _getStatusMessage(int code) const;
        void _addDefaultHeaders();
        
        // Header validation methods
        bool _isValidHeaderName(const std::string& name) const;
        bool _isValidHeaderValue(const std::string& value) const;
        std::string _sanitizeHeaderValue(const std::string& value) const;
        
    public:
        Response();
        Response(int statusCode);
        ~Response();

        // Core methods
        void setStatus(int code);
        void addHeader(const std::string& key, const std::string& value);
        void setBody(const std::string& bodyContent);
        void setBody(const std::vector<char>& bodyContent);
        std::string build();
        
        // Performance optimization methods
        void setFilePath(const std::string& path);
        void setStreaming(bool streaming);
        void setFileSize(size_t size);
        const std::string& getFilePath() const { return _filePath; }
        bool isStreaming() const { return _isStreaming; }
        size_t getFileSize() const { return _fileSize; }
        
        // Getters
        int getStatusCode() const { return _statusCode; }
        const std::string& getStatusMessage() const { return _statusMessage; }
        const std::string& getBody() const { return _body; }
        const std::map<std::string, std::string>& getHeaders() const { return _headers; }
        
        // Utility methods
        void clear();
        bool isEmpty() const;
        size_t getContentLength() const;
        
        // HTTP-specific methods
        void setContentType(const std::string& type);
        void setContentLength(size_t length);
        void setConnection(const std::string& connection);
        void setLocation(const std::string& location);
        void setAllow(const std::string& methods);
        
        // Error response helpers
        static Response createErrorResponse(int statusCode, const std::string& message = "");
        static Response createRedirectResponse(int statusCode, const std::string& location);
        
        // CGI response handling
        void mergeCGIHeaders(const std::map<std::string, std::string>& cgiHeaders);
        void setCGIStatus(int statusCode);
        
        // Chunked transfer encoding support
        void setChunkedEncoding(bool chunked);
        bool isChunkedEncoding() const { return _isChunked; }
        std::string buildHeadersOnly() const;
        std::string buildChunk(const std::string& data) const;
        std::string buildFinalChunk() const;
};
