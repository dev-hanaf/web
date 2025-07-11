#include <cstdlib>
# include <ctime>
# include <unistd.h>
# include <sys/epoll.h>
#include "conf/AutoIndex.hpp"
# include "conf/Http.hpp"
# include "conf/Root.hpp"
# include "Connection.hpp"
# include "conf/Listen.hpp"
# include "conf/Server.hpp"
# include "conf/Location.hpp"
# include "conf/IDirective.hpp"
# include "conf/ServerName.hpp"
# include "conf/LimitExcept.hpp"
# include "request/incs/Defines.hpp"
# include "conf/ClientMaxBodySize.hpp"

Connection::Connection()
	:	fd(-1), req(NULL), connect(false),
		conServer(NULL), shouldKeepAlive(false), lastTimeoutCheck(time(NULL))
{
}

Connection::Connection(int clientFd)
	:	fd(clientFd), req(NULL), connect(false),
		conServer(NULL), shouldKeepAlive(false), lastTimeoutCheck(time(NULL))
{
}

void	Connection::updateTime()
{
	lastTimeoutCheck = time(NULL);
}

Connection* Connection::findConnectionByFd(int fd, std::vector<Connection*>& connections)
{
	for (std::vector<Connection*>::iterator it = connections.begin(); 
		 it != connections.end(); ++it)
	{
		if ((*it)->fd == fd)
			return *it;
	}
	return NULL;
}

static std::vector<Server*> getServersFromHttp(Http* http)
{
	std::vector<Server*> servers;
	for (size_t i = 0; i < http->directives.size(); ++i) {
		if (http->directives[i]->getType() == SERVER) {
			servers.push_back(static_cast<Server*>(http->directives[i]));
		}
	}
	return servers;
}

bool	Connection::findServer(Http *http)
{
	if (!req || req->getState() < HEADERS || !req->getRequestHeaders().hasHeader("host"))
		return false;
	if (req->getState() >= HEADERS && !req->getRequestHeaders().hasHeader("host"))
		return false;

	unsigned int port = req->getRequestHeaders().getHostPort();
	std::string hostname = req->getRequestHeaders().getHostName();
	std::vector<Server*> servers = getServersFromHttp(http);
	
	if (servers.empty())
		return false;
		
	for (std::vector<Server*>::const_iterator it = servers.begin(); it != servers.end(); ++it) {
		Server* server = *it;
		if (!server) continue;
		
		for (std::vector<IDirective*>::const_iterator dit = server->directives.begin(); dit != server->directives.end(); ++dit)
		{
			if (!(*dit)) continue;
			
			if ((*dit)->getType() == SERVER_NAME)
			{
				ServerName* sn = static_cast<ServerName*>(*dit);
				if (sn) {
					char** names = sn->getServerNames();
					if (names)
					{
						for (unsigned int i = 0; names[i] != NULL; ++i)
							if (names[i] == hostname)
							{
								conServer = server;
								return true;
							}
					}
				}
			}
			else if ((*dit)->getType() == LISTEN)
			{
				Listen* listen = static_cast<Listen*>(*dit);
				if (listen && listen->getPort() == port)
				{
					conServer = server;
					return true;
				}
			}
		}
	}
	if (conServer == NULL)
		conServer = servers[0];
	return false;
}

IDirective*	Connection::getDirective(DIRTYPE type)
{
	if (conServer == NULL || conServer->directives.empty())
		return NULL;

	for (std::vector<IDirective*>::iterator dit = conServer->directives.begin(); 
		 dit != conServer->directives.end(); ++dit) 
	{
		if ((*dit) && (*dit)->getType() == type)
			return *dit;
	}
	return NULL;
}

LimitExcept*	Connection::getLimitExcept()
{
	const Location*	location = getLocation();
	if (location == NULL)
		return NULL;
	
	for (std::vector<IDirective*>::const_iterator dit = location->directives.begin(); 
	dit != location->directives.end(); ++dit) 
	{
		if ((*dit)->getType() == LIMIT_EXCEPT) {
			LimitExcept* limitExcept = static_cast<LimitExcept*>(*dit);
			if (limitExcept) return limitExcept;
		}
	}
	return NULL;
}

bool	Connection::checkMaxBodySize()
{
	ClientMaxBodySize* max = getClientMaxBodySize();
	if (max == NULL)
		return true;

	std::string str = req->getRequestHeaders().getHeaderValue("content-length");
	if (str.empty())
		return true; // No content-length header, assume OK

	unsigned int actualSize = strtoull(str.c_str(), NULL, 10);

	if (max->getSize() < actualSize)
		return false;

	return true;
}

ClientMaxBodySize*	Connection::getClientMaxBodySize()
{
	ClientMaxBodySize* maxSize = static_cast<ClientMaxBodySize*>(getDirective(CLIENT_MAX_BODY_SIZE));
	if (maxSize) return maxSize;
	
	// Check location context if not found in server
	const Location* location = getLocation();
	if (location) {
		for (std::vector<IDirective*>::const_iterator dit = location->directives.begin(); 
		dit != location->directives.end(); ++dit) 
		{
			if ((*dit)->getType() == CLIENT_MAX_BODY_SIZE) {
				ClientMaxBodySize* locMaxSize = static_cast<ClientMaxBodySize*>(*dit);
				if (locMaxSize) return locMaxSize;
			}
		}
	}
	return NULL;
}

const Location* Connection::getLocation() const
{
    if (!conServer)
        return NULL;
    std::string reqUri;
    if (req && req->getRequestLine().getUri().size())
        reqUri = req->getRequestLine().getUri();
    else if (!uri.empty())
        reqUri = uri;
    else
        return NULL;

    // First, check for exact match (file-level or exact location)
    for (std::vector<IDirective*>::const_iterator it = conServer->directives.begin(); it != conServer->directives.end(); ++it) {
        if ((*it)->getType() != LOCATION)
            continue;
        const Location* loc = static_cast<const Location*>(*it);
        if (!loc) continue;
        char* locUri = loc->getUri();
        if (!locUri) continue;
        std::string locUriStr(locUri);
        if (locUriStr == reqUri) {
            // std::cout << "[getLocation] Exact file/location match found!" << std::endl;
            return loc;
        }
    }

    // Then, check for best prefix match
    const Location* bestLoc = NULL;
    size_t bestMatchLen = 0;
    for (std::vector<IDirective*>::const_iterator it = conServer->directives.begin(); it != conServer->directives.end(); ++it) {
        if ((*it)->getType() != LOCATION)
            continue;
        const Location* loc = static_cast<const Location*>(*it);
        if (!loc) continue;
        char* locUri = loc->getUri();
        bool exact = loc->isExactMatch();
        if (!locUri) continue;
        std::string locUriStr(locUri);
        if (exact) {
            if (reqUri == locUriStr) {
                // std::cout << "[getLocation] Exact match found!" << std::endl;
                return loc;
            }
        } else {
            if (!locUriStr.empty() && reqUri.find(locUriStr) == 0 && locUriStr.length() > bestMatchLen) {
                bestMatchLen = locUriStr.length();
                bestLoc = loc;
            }
        }
    }
    if (bestLoc) {
        // std::cout << "[getLocation] Prefix match found!" << std::endl;
        return bestLoc;
    }
    // std::cout << "[getLocation] No match found." << std::endl;
    return NULL;
}

const Location* Connection::getLocation()
{
	if (!conServer)
		return NULL;
	std::string reqUri;
	if (req && req->getRequestLine().getUri().size())
		reqUri = req->getRequestLine().getUri();
	else if (!uri.empty())
		reqUri = uri;
	else
		return NULL;

	const Location* bestLoc = NULL;
	size_t bestMatchLen = 0;
	for (std::vector<IDirective*>::const_iterator it = conServer->directives.begin(); it != conServer->directives.end(); ++it) {
		if ((*it)->getType() != LOCATION)
			continue;
		const Location* loc = static_cast<const Location*>(*it);
		if (!loc) continue;
		char* locUri = loc->getUri();
		bool exact = loc->isExactMatch();
		if (!locUri) continue;
		std::string locUriStr(locUri);
		// std::cout << "[getLocation] reqUri: '" << reqUri << "' locUri: '" << locUriStr << "' exact: " << exact << std::endl;
		if (exact) {
			if (reqUri == locUriStr) {
				// std::cout << "[getLocation] Exact match found!" << std::endl;
				return loc;
			}
		} else {
			if (!locUriStr.empty() && reqUri.find(locUriStr) == 0 && locUriStr.length() > bestMatchLen) {
				bestMatchLen = locUriStr.length();
				bestLoc = loc;
			}
		}
	}
	if (bestLoc) {
		// std::cout << "[getLocation] Prefix match found!" << std::endl;
		return bestLoc;
	}
	// std::cout << "[getLocation] No match found." << std::endl;
	return NULL;
}

Root*	Connection::getRoot()
{
	Root*	root = static_cast<Root*>(getDirective(ROOT));
	if (root)
		return root;
	const Location*	location = getLocation();
	if (location == NULL)
		return NULL;
	
	for (std::vector<IDirective*>::const_iterator dit = location->directives.begin(); 
	dit != location->directives.end(); ++dit) 
	{
		if ((*dit)->getType() == ROOT) {
			Root* locRoot = static_cast<Root*>(*dit);
			if (locRoot) return locRoot;
		}
	}
	return NULL;
}

AutoIndex* Connection::getAutoIndex()
{
    // Check location context first
    const Location* location = getLocation();
    if (location) {
        // std::cout << "[DEBUG] Location directives for URI: ";
        char* locUri = location->getUri();
        if (locUri) std::cout << locUri << std::endl;
        for (std::vector<IDirective*>::const_iterator dit = location->directives.begin(); 
        dit != location->directives.end(); ++dit) 
        {
            // std::cout << "[DEBUG] Location directive type: " << (*dit)->getType() << std::endl;
            if ((*dit)->getType() == AUTOINDEX) {
                AutoIndex* locAutoIndex = static_cast<AutoIndex*>(*dit);
                if (locAutoIndex) {
                    // std::cout << "autoindex (location) -> " << locAutoIndex->getState() << std::endl;
                    return locAutoIndex;
                }
            }
        }
    }
    // Then check server context
    AutoIndex* autoindex = static_cast<AutoIndex*>(getDirective(AUTOINDEX));
    if (autoindex) {
        // std::cout << "autoindex  -> " << autoindex->getState() << std::endl;
        return autoindex;
    }
    // Default to off
    static AutoIndex defaultOff;
    defaultOff.setState(false);
    return &defaultOff;
}

Return* Connection::getReturnDirective() const {
	const Location* location = getLocation();
	// std::cout << "location -> " << location << std::endl;
	if (location) {
		IDirective* dir = location->getDirective(RETURN);
		if (dir) {
			Return* ret = static_cast<Return*>(dir);
			if (ret) return ret;
		}
	}
	if (conServer) {
		for (std::vector<IDirective*>::const_iterator dit = conServer->directives.begin(); dit != conServer->directives.end(); ++dit) {
			if ((*dit)->getType() == RETURN) {
				Return* ret = static_cast<Return*>(*dit);
				if (ret) return ret;
			}
		}
	}
	return NULL;
}

Index* Connection::getIndex() {
	Index* index = static_cast<Index*>(getDirective(INDEX));
	if (index)
		return index;
	
	// Check location context if not found in server
	const Location* location = getLocation();
	if (location) {
		for (std::vector<IDirective*>::const_iterator dit = location->directives.begin(); 
		dit != location->directives.end(); ++dit) 
		{
			if ((*dit)->getType() == INDEX) {
				return static_cast<Index*>(*dit);
			}
		}
	}
	return NULL;
}

ErrorPage* Connection::getErrorPage() {
	ErrorPage* errorPage = static_cast<ErrorPage*>(getDirective(ERROR_PAGE));
	if (errorPage)
		return errorPage;
	
	// Check location context if not found in server
	const Location* location = getLocation();
	if (location) {
		for (std::vector<IDirective*>::const_iterator dit = location->directives.begin(); 
		dit != location->directives.end(); ++dit) 
		{
			if ((*dit)->getType() == ERROR_PAGE) {
				ErrorPage* locErrorPage = static_cast<ErrorPage*>(*dit);
				if (locErrorPage) return locErrorPage;
			}
		}
	}
	return NULL;
}

ErrorPage* Connection::getErrorPageForCode(int code) const {
	const Location* location = getLocation();
	if (location) {
		for (std::vector<IDirective*>::const_iterator dit = location->directives.begin(); dit != location->directives.end(); ++dit) {
			if ((*dit)->getType() == ERROR_PAGE) {
				ErrorPage* ep = static_cast<ErrorPage*>(*dit);
				if (ep && (ep->getCode() == code || ep->getResponseCode() == code))
					return ep;
			}
		}
	}
	if (conServer) {
		for (std::vector<IDirective*>::const_iterator dit = conServer->directives.begin(); dit != conServer->directives.end(); ++dit) {
			if ((*dit)->getType() == ERROR_PAGE) {
				ErrorPage* ep = static_cast<ErrorPage*>(*dit);
				if (ep && (ep->getCode() == code || ep->getResponseCode() == code))
					return ep;
			}
		}
	}
	return NULL;
}

void	Connection::closeConnection(Connection* conn, std::vector<Connection*>& connections, int epollFd)
{
	epoll_ctl(epollFd, EPOLL_CTL_DEL, conn->fd, NULL);

	if (conn->fd != -1)
		close(conn->fd);

	if (conn->req)
	{
		delete conn->req;
		conn->req = NULL;
	}
	// if (conn.res)
	// 	delete conn.res;

	for (std::vector<Connection*>::iterator it = connections.begin(); it != connections.end(); ++it)
	{
		if (*it == conn)
		{
			connections.erase(it);
			std::cout << "connection erased" << (*it)->fd << std::endl; 
			break;
		}
	}

	delete conn;
}
