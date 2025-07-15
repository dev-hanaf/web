#include "../include/ReturnHandler.hpp"
#include "../include/ErrorResponse.hpp"
#include "../include/FileResponse.hpp"
#include "../../Connection.hpp"
#include <sys/stat.h>
#include <string>

Response ReturnHandler::handle(Connection* conn) {
    Return* ret = conn ? conn->getReturnDirective() : NULL;
    if (!ret) return Response();
    const Location* loc = conn->getLocation();
    std::string reqUri = conn->req->getRequestLine().getUri();
    bool applyReturn = false;
    if (loc && loc->getDirective(RETURN) == ret) {
        std::string locUri = loc->getUri() ? std::string(loc->getUri()) : "";
        std::cout << RED << "location uri -> " << locUri << std::endl;
        if (loc->isExactMatch()) {
            if (reqUri == locUri) applyReturn = true;
        } else {
            if (!locUri.empty() && reqUri.find(locUri) == 0) applyReturn = true;
        }
    } else {
        applyReturn = true;
    }
    if (applyReturn) {
        unsigned int code = ret->getCode();
        char* url = ret->getUrl();
        if (code >= 300 && code < 400 && url && url[0]) {
            return Response::createRedirectResponse(code, std::string(url));
        } else if (code >= 400 && code < 600) {
            if (url && url[0] == '/') {
                std::string root = conn && conn->getRoot() && conn->getRoot()->getPath() ? std::string(conn->getRoot()->getPath()) : "www";
                std::string filePath = root;
                if (filePath[filePath.length() - 1] != '/') filePath += "/";
                std::string urlStr(url);
                if (urlStr[0] == '/') urlStr = urlStr.substr(1);
                filePath += urlStr;
                struct stat fileStat;
                if (stat(filePath.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode)) {
                    return FileResponse::serve(filePath, "text/html", code);
                }
            }
            return ErrorResponse::createErrorResponseWithMapping(conn, code, url && url[0] ? std::string(url) : "");
        } else {
            return ErrorResponse::createErrorResponseWithMapping(conn, code, url && url[0] ? std::string(url) : "");
        }
    }
    return Response();
} 