#include "../include/ErrorResponse.hpp"
#include "../../Connection.hpp"
#include "../../conf/ErrorPage.hpp"
#include <sys/stat.h>
Response ErrorResponse::createErrorResponse(int statusCode, const std::string& message) {
    (void)message;
    Response response(statusCode);
    response.setContentType("text/html");
    response.setFileBody("");
    return response;
}
Response ErrorResponse::createErrorResponseWithMapping(Connection* conn, int statusCode, const std::string& message) {
    if (conn) {
        ErrorPage* ep = conn->getErrorPageForCode(statusCode);
        if (ep && ep->getUri() && ep->getUri()[0]) {
            std::string root = conn->getRoot() && conn->getRoot()->getPath() ? std::string(conn->getRoot()->getPath()) : "www";
            std::string errorPath = root;
            if (errorPath[errorPath.length() - 1] != '/') errorPath += "/";
            std::string uriStr(ep->getUri());
            if (uriStr[0] == '/') uriStr = uriStr.substr(1);
            errorPath += uriStr;
            struct stat fileStat;
            if (stat(errorPath.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode)) {
                Response response(statusCode);
                response.setFilePath(errorPath);
                response.setContentType("text/html");
                response.setFileSize(static_cast<size_t>(fileStat.st_size));
                return response;
            }
        }
    }
    return createErrorResponse(statusCode, message);
}
Response ErrorResponse::createMethodNotAllowedResponse(const std::vector<std::string>& allowedMethods) {
    Response response(405);
    std::string methods;
    for (size_t i = 0; i < allowedMethods.size(); ++i) {
        if (i > 0) methods += ", ";
        methods += allowedMethods[i];
    }
    response.addHeader("Allow", methods);
    response.setContentType("text/html");
    response.setFileBody("");
    return response;
}
Response ErrorResponse::createNotFoundResponse(Connection* conn) {
    return createErrorResponseWithMapping(conn, 404, "Not Found");
}
Response ErrorResponse::createForbiddenResponse() {
    return createErrorResponse(403, "Forbidden");
}
Response ErrorResponse::createInternalErrorResponse() {
    return createErrorResponse(500, "Internal Server Error");
} 