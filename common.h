#ifndef COMMON_H
#define COMMON_H

#include <string>

const int DEFAULT_PORT = 8888;
const int BUFFER_SIZE = 4096;

const std::string CMD_LOGIN = "LOGIN";
const std::string CMD_MSG = "MSG";
const std::string CMD_QUIT = "QUIT";
const std::string CMD_INMSG = "INMSG";
const std::string CMD_ERROR = "ERROR";
const std::string CMD_OK = "OK";

#endif