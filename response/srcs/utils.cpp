#include "../include/utils.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <cerrno>
#include <cstring> // for memset

std::string loadFile(const std::string& path) {
    // Ensure tmp/response directory exists
    std::string dir = "tmp/response";
    struct stat st;
    memset(&st, 0, sizeof(st));
    if (stat(dir.c_str(), &st) == -1) {
        mkdir("tmp", 0755); // Try to create tmp if not exists
        mkdir(dir.c_str(), 0755);
    }

    // Generate random filename
    std::srand(std::time(0) ^ getpid());
    std::stringstream ss;
    ss << dir << "/resp_" << std::rand() << ".tmp";
    std::string filename = ss.str();

    std::ifstream src(path.c_str(), std::ios::binary);
    if (!src.is_open())
        return "";
    std::ofstream dst(filename.c_str(), std::ios::binary);
    if (!dst.is_open())
        return "";
    dst << src.rdbuf();
    src.close();
    dst.close();
    return filename;
}

std::string getMimeType(const std::string& path) {
	if (path.find(".html") != std::string::npos) return "text/html";
	if (path.find(".css") != std::string::npos) return "text/css";
	if (path.find(".js") != std::string::npos) return "application/javascript";
	if (path.find(".png") != std::string::npos) return "image/png";
	return "text/plain";
}
