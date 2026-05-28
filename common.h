#ifndef COMMON_H
#define COMMON_H

#include <string>

const int DEFAULT_PORT = 8888;
const int BUFFER_SIZE = 4096;

// Константы команд сетевого протокола
const std::string CMD_LOGIN = "LOGIN";
const std::string CMD_QUIT = "QUIT";
const std::string CMD_MSG = "MSG";
const std::string CMD_OK = "OK";
const std::string CMD_ERROR = "ERROR";
const std::string CMD_INMSG = "INMSG";

const std::string CMD_GROUP_JOIN = "GJOIN";
const std::string CMD_GROUP_MSG = "GMSG";
const std::string CMD_GROUP_ADD = "GADD";
const std::string CMD_GROUP_KICK = "GKICK";
const std::string CMD_GROUP_DEL = "GDEL";
const std::string CMD_GROUP_NOTIFY = "GNOTIFY";

const std::string CMD_REQ_HISTORY = "REQHIST";
const std::string CMD_HIST_LINE = "HISTLINE";

#endif