#include "../include/ReturnHandler.hpp"
#include "../../Connection.hpp"
#include "../../conf/Return.hpp"
Response ReturnHandler::handle(Connection* conn) {
    Return* ret = conn ? conn->getReturnDirective() : NULL;
    if (!ret) return Response();
    unsigned int code = ret->getCode();
    char* url = ret->getUrl();
    Response response(code);
    if (url && url[0]) {
        response.addHeader("Location", std::string(url));
    }
    return response;
} 