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
        std::string _filePath;  // For direct file serving
        size_t _fileSize;       // File size for Content-Length
        bool _isBuilt;
        std::string _cachedResponse;
        void _addDefaultHeaders();
        std::string _getStatusMessage(int code) const;
    public:
        Response();
        Response(int statusCode);
        ~Response();
        void setStatus(int code);
        void addHeader(const std::string& key, const std::string& value);
        void setFilePath(const std::string& path);
        void setFileBody(const std::string& filePath);
        void setFileSize(size_t size);
        void setContentType(const std::string& type);
        std::string build();
        int getStatusCode() const { return _statusCode; }
        const std::string& getStatusMessage() const { return _statusMessage; }
        const std::string& getFilePath() const { return _filePath; }
        size_t getFileSize() const { return _fileSize; }
        void clear();
};
