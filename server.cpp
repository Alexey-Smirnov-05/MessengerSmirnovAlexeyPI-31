#include <iostream>      // Для вывода в консоль (std::cout, std::cerr, std::endl)
#include <string>        // Для работы со строками типа std::string
#include <cstring>       // Для работы с C-строками и функциями memset, strcpy
#include <fstream>       // Для работы с файлами (чтение историй сообщений и конфигов)
#include <map>           // Для хранения данных в виде пар "ключ-значение" (словари)
#include <set>           // Для хранения уникальных элементов (списки участников групп)
#include <sstream>       // Для парсинга строк через строковые потоки (std::istringstream)
#include <algorithm>     // Для алгоритмов вроде std::swap и std::remove
#include <thread>        // Для создания и управления параллельными потоками
#include <mutex>         // Для синхронизации потоков (взаимное исключение — мьютекс)
#include <signal.h>      // Для обработки системных сигналов (SIGINT, SIGHUP)
#include <csignal>       // Стандартный C++ аналог signal.h
#include <errno.h>       // Для работы с кодами системных ошибок
#include <filesystem>    // Для работы с файловой системой (удаление файлов)

#include <openssl/ssl.h> // Главная библиотека OpenSSL для безопасного соединения
#include <openssl/err.h> // Для получения расшифровки ошибок OpenSSL

#include "common.h"      // Твой кастомный файл с общими макросами
#include "logger.h"      // Твой кастомный файл с функцией логирования logMessage

namespace fs = std::filesystem; // Глобальные пространства имен и константы

const std::string SERVER_LOG = "logs/server.log";
const std::string GROUPS_CONFIG_FILE = "groups/groups_config.txt";

// Гарантируем наличие новых макросов
#ifndef CMD_REQ_GROUP_USERS
#  define CMD_REQ_GROUP_USERS "REQ_GROUP_USERS"
#endif
#ifndef CMD_LIST_GROUP_USERS
#  define CMD_LIST_GROUP_USERS "LIST_GROUP_USERS"
#endif

// Структуры данных и глобальное состояние сервера
struct ClientInfo {
    std::string username; // имя пользователя
    SSL* ssl;             // указатель на SSL-структуру OpenSSL
};

std::map<SOCKET, ClientInfo> clients; // Ассоциативный массив подключенных клиентов

// Group — структура для описания чат-группы.
struct Group {
    std::string admin;
    std::set<std::string> members;
};
std::map<std::string, Group> groups;

std::mutex stateMtx;
volatile sig_atomic_t server_running = 1; // флаг работы сервера.
volatile sig_atomic_t reload_config = 0;
SOCKET server_fd = INVALID_SOCKET;        // Главный слушающий сокет сервера
SSL_CTX* server_ctx = nullptr;            // Указатель на контекст OpenSSL

void handle_sigint(int) {
    server_running = 0;
    if (server_fd != INVALID_SOCKET) {
        closesocket(server_fd);
    }
}

#ifndef _WIN32
void handle_sighup(int) {
    reload_config = 1;
}
#endif

// Функция очистки имен файлов (защита от directory traversal)
std::string sanitizeForFilename(std::string name) {
    std::replace(name.begin(), name.end(), '/', '_');
    std::replace(name.begin(), name.end(), '\\', '_');
    return name;
}

// Теперь файлы истории ЛС хранятся строго в папке chats/
std::string getPMFilename(std::string u1, std::string u2) {
    if (u1 > u2) std::swap(u1, u2);
    return "chats/history_pm_" + sanitizeForFilename(u1) + "_" + sanitizeForFilename(u2) + ".txt";
}

// Теперь файлы истории Групп хранятся строго в папке chats/
std::string getGroupFilename(std::string gname) {
    std::string clean = sanitizeForFilename(gname);
    if (!clean.empty() && clean[0] == '#') {
        return "chats/history_group_" + clean.substr(1) + ".txt";
    }
    return "chats/history_group_" + clean + ".txt";
}

void saveGroupsConfig() {
    std::ofstream f(GROUPS_CONFIG_FILE);
    if (!f.is_open()) return;
    for (const auto& pair : groups) {
        f << pair.first << "|" << pair.second.admin << "|";
        bool first = true;
        for (const auto& m : pair.second.members) {
            if (!first) f << ",";
            f << m;
            first = false;
        }
        f << "\n";
    }
}

void loadGroupsConfig() {
    groups.clear();
    std::ifstream f(GROUPS_CONFIG_FILE);
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream iss(line);
        std::string gname, admin, mems;
        if (std::getline(iss, gname, '|') && std::getline(iss, admin, '|')) {
            Group g;
            g.admin = admin;
            if (std::getline(iss, mems)) {
                std::istringstream memiss(mems);
                std::string member;
                while (std::getline(memiss, member, ',')) {
                    if (!member.empty()) g.members.insert(member);
                }
            }
            groups[gname] = g;
        }
    }
    std::cout << "[Server] Loaded/Reloaded " << groups.size() << " groups from backup file." << std::endl;
}

void appendPMHistory(const std::string& u1, const std::string& u2, const std::string& sender, const std::string& msg) {
    std::ofstream f(getPMFilename(u1, u2), std::ios::app);
    if (f.is_open()) {
        f << sender << "|" << msg << "\n";
    }
}

void appendGroupHistory(const std::string& gname, const std::string& sender, const std::string& msg) {
    std::ofstream f(getGroupFilename(gname), std::ios::app);
    if (f.is_open()) {
        f << sender << "|" << msg << "\n";
    }
}

bool sendToClient(SSL* ssl, const std::string& message) {
    if (!ssl) return false;
    std::string msg = message + "\n";
    int sent = SSL_write(ssl, msg.c_str(), static_cast<int>(msg.length()));
    return sent > 0;
}

void sendPMHistoryToClient(SSL* ssl, const std::string& u1, const std::string& u2) {
    std::ifstream f(getPMFilename(u1, u2));
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) sendToClient(ssl, std::string(CMD_HIST_LINE) + "|PM|" + line);
    }
}

void sendGroupHistoryToClient(SSL* ssl, const std::string& gname) {
    std::ifstream f(getGroupFilename(gname));
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) sendToClient(ssl, std::string(CMD_HIST_LINE) + "|GROUP|" + gname + "|" + line);
    }
}

void processClientCommand(SOCKET client_sock, SSL* ssl, const std::string& data) {
    std::istringstream iss(data);
    std::string cmd;
    std::getline(iss, cmd, '|');

    if (cmd == CMD_LOGIN) {
        std::string name;
        std::getline(iss, name);

        stateMtx.lock();
        bool nameExists = false;
        for (auto& p : clients) {
            if (p.second.username == name) {
                nameExists = true;
                break;
            }
        }
        if (nameExists) {
            sendToClient(ssl, std::string(CMD_ERROR) + "|Username already taken");
        }
        else {
            clients[client_sock].username = name;
            sendToClient(ssl, std::string(CMD_OK) + "|Logged in as " + name);
            logMessage(SERVER_LOG, "INFO", "User " + name + " connected securely");
            std::cout << "[Server] User " << name << " connected securely" << std::endl;
        }
        stateMtx.unlock();
    }
    else if (cmd == CMD_QUIT) {
        stateMtx.lock();
        std::string name = clients[client_sock].username;
        stateMtx.unlock();
        sendToClient(ssl, std::string(CMD_OK) + "|Goodbye!");
        logMessage(SERVER_LOG, "INFO", "User " + name + " requested disconnect (QUIT)");
    }
    else if (cmd == CMD_REQ_ONLINE) {
        stateMtx.lock();
        std::string online_list = "";
        bool first = true;
        for (const auto& p : clients) {
            if (!p.second.username.empty()) {
                if (!first) online_list += ",";
                online_list += p.second.username;
                first = false;
            }
        }
        stateMtx.unlock();
        sendToClient(ssl, std::string(CMD_ONLINE_LIST) + "|" + online_list);
    }
    else if (cmd == CMD_REQ_GROUP_USERS || cmd == "REQ_GROUP_USERS") {
        std::string req_group;
        std::getline(iss, req_group);
        if (!req_group.empty() && req_group.back() == '\r') req_group.pop_back();

        stateMtx.lock();
        if (groups.find(req_group) != groups.end()) {
            std::stringstream ss;
            ss << CMD_LIST_GROUP_USERS << "|";
            bool firstUser = true;
            for (const auto& member : groups[req_group].members) {
                if (!firstUser) ss << ",";
                ss << member;
                firstUser = false;
            }
            sendToClient(ssl, ss.str());
        }
        else {
            sendToClient(ssl, std::string(CMD_ERROR) + "|Group not found");
        }
        stateMtx.unlock();
    }
    else if (cmd == CMD_CLEAR) {
        std::string type, target;
        std::getline(iss, type, '|');
        std::getline(iss, target);

        stateMtx.lock();
        std::string sender = clients[client_sock].username;
        stateMtx.unlock();

        if (type == "PM") {
            std::string filename = getPMFilename(sender, target);
            fs::remove(filename);
            logMessage(SERVER_LOG, "INFO", "User " + sender + " cleared PM history with " + target);
            sendToClient(ssl, std::string(CMD_OK) + "|Private chat history cleared.");
        }
        else if (type == "GROUP") {
            stateMtx.lock();
            bool isAdmin = (groups.find(target) != groups.end() && groups[target].admin == sender);
            stateMtx.unlock();

            if (isAdmin) {
                std::string filename = getGroupFilename(target);
                fs::remove(filename);
                logMessage(SERVER_LOG, "GROUP", "Admin " + sender + " cleared history for group " + target);

                sendToClient(ssl, std::string(CMD_OK) + "|Group chat history cleared.");

                stateMtx.lock();
                for (const auto& member : groups[target].members) {
                    for (auto& c : clients) {
                        if (c.second.username == member) {
                            sendToClient(c.second.ssl, std::string(CMD_GROUP_MSG) + "|" + target + "|Server|Chat history was cleared by admin.");
                        }
                    }
                }
                stateMtx.unlock();
            }
            else {
                sendToClient(ssl, std::string(CMD_ERROR) + "|Access denied: Only group admin can clear group history.");
            }
        }
    }
    else if (cmd == CMD_MSG) {
        std::string target, msg;
        std::getline(iss, target, '|');
        std::getline(iss, msg);

        stateMtx.lock();
        std::string sender = clients[client_sock].username;

        if (msg.find("_SYSTEM_CHAT_CLEAR_REQUEST_") == std::string::npos) {
            appendPMHistory(sender, target, sender, msg);
            logMessage(SERVER_LOG, "MSG", "Private: " + sender + " -> " + target + ": " + msg);
        }

        SSL* target_ssl = nullptr;
        for (auto& p : clients) {
            if (p.second.username == target) {
                target_ssl = p.second.ssl;
                break;
            }
        }
        if (target_ssl == nullptr) {
            if (msg.find("_SYSTEM_CHAT_CLEAR_REQUEST_") == std::string::npos) {
                sendToClient(ssl, std::string(CMD_OK) + "|Message saved (User is offline)");
            }
        }
        else {
            sendToClient(target_ssl, std::string(CMD_INMSG) + "|" + sender + "|" + msg);
            if (msg.find("_SYSTEM_CHAT_CLEAR_REQUEST_") == std::string::npos) {
                sendToClient(ssl, std::string(CMD_OK) + "|Message sent");
            }
        }
        stateMtx.unlock();
    }
    else if (cmd == CMD_REQ_HISTORY) {
        std::string type, target;
        std::getline(iss, type, '|');
        std::getline(iss, target);

        stateMtx.lock();
        std::string sender = clients[client_sock].username;
        stateMtx.unlock();

        if (type == "PM") {
            sendPMHistoryToClient(ssl, sender, target);
        }
        else if (type == "GROUP") {
            sendGroupHistoryToClient(ssl, target);
        }
    }
    else if (cmd == CMD_GROUP_JOIN) {
        std::string group_name;
        std::getline(iss, group_name);

        stateMtx.lock();
        std::string sender = clients[client_sock].username;
        if (groups.find(group_name) == groups.end()) {
            Group g;
            g.admin = sender;
            g.members.insert(sender);
            groups[group_name] = g;
            saveGroupsConfig();

            sendToClient(ssl, std::string(CMD_OK) + "|Group created. You are admin.");
            logMessage(SERVER_LOG, "GROUP", sender + " created group " + group_name);
            sendGroupHistoryToClient(ssl, group_name);
        }
        else {
            if (groups[group_name].members.count(sender)) {
                sendToClient(ssl, std::string(CMD_OK) + "|Joined group.");
                logMessage(SERVER_LOG, "GROUP", sender + " entered group " + group_name);
                sendGroupHistoryToClient(ssl, group_name);
            }
            else {
                sendToClient(ssl, std::string(CMD_ERROR) + "|Access denied: You are not a member of this group. Admin must add you.");
            }
        }
        stateMtx.unlock();
    }
    else if (cmd == CMD_GROUP_MSG) {
        std::string group_name, msg;
        std::getline(iss, group_name, '|');
        std::getline(iss, msg);

        stateMtx.lock();
        std::string sender = clients[client_sock].username;
        if (groups.find(group_name) != groups.end() && groups[group_name].members.count(sender)) {
            if (msg.find("_SYSTEM_CHAT_CLEAR_REQUEST_") == std::string::npos) {
                appendGroupHistory(group_name, sender, msg);
                logMessage(SERVER_LOG, "GMSG", "[" + group_name + "] " + sender + ": " + msg);
            }

            for (const auto& member : groups[group_name].members) {
                for (auto& c : clients) {
                    if (c.second.username == member && c.first != client_sock) {
                        sendToClient(c.second.ssl, std::string(CMD_GROUP_MSG) + "|" + group_name + "|" + sender + "|" + msg);
                    }
                }
            }
        }
        else {
            sendToClient(ssl, std::string(CMD_ERROR) + "|Access denied or group not found");
        }
        stateMtx.unlock();
    }
    else if (cmd == CMD_GROUP_ADD) {
        std::string group_name, target;
        std::getline(iss, group_name, '|');
        std::getline(iss, target);

        stateMtx.lock();
        std::string sender = clients[client_sock].username;
        if (groups.find(group_name) != groups.end() && groups[group_name].admin == sender) {
            groups[group_name].members.insert(target);
            saveGroupsConfig();

            sendToClient(ssl, std::string(CMD_OK) + "|User " + target + " added.");
            logMessage(SERVER_LOG, "GROUP", sender + " added " + target + " to " + group_name);

            // БРОДКАСТ ВСЕМ: Оповещаем всех участников, что был добавлен новый пользователь
            for (const auto& member : groups[group_name].members) {
                for (auto& c : clients) {
                    if (c.second.username == member) {
                        sendToClient(c.second.ssl, std::string(CMD_GROUP_NOTIFY) + "|ADDED|" + group_name + "|" + sender + "|" + target);
                    }
                }
            }
        }
        else {
            sendToClient(ssl, std::string(CMD_ERROR) + "|Only admin can add members");
        }
        stateMtx.unlock();
    }
    else if (cmd == CMD_GROUP_KICK) {
        std::string group_name, target;
        std::getline(iss, group_name, '|');
        std::getline(iss, target);

        stateMtx.lock();
        std::string sender = clients[client_sock].username;
        if (groups.find(group_name) != groups.end() && groups[group_name].admin == sender) {
            if (groups[group_name].members.erase(target)) {
                saveGroupsConfig();

                sendToClient(ssl, std::string(CMD_OK) + "|User " + target + " deleted.");
                logMessage(SERVER_LOG, "GROUP", sender + " kicked " + target + " from " + group_name);

                // Оповещаем оставшихся участников о кике
                for (const auto& member : groups[group_name].members) {
                    for (auto& c : clients) {
                        if (c.second.username == member) {
                            sendToClient(c.second.ssl, std::string(CMD_GROUP_NOTIFY) + "|KICKED|" + group_name + "|" + sender + "|" + target);
                        }
                    }
                }
                // Отдельно оповещаем исключенного, чтобы у него вывелось предупреждение и закрылся чат
                for (auto& c : clients) {
                    if (c.second.username == target) {
                        sendToClient(c.second.ssl, std::string(CMD_GROUP_NOTIFY) + "|KICKED|" + group_name + "|" + sender + "|" + target);
                        break;
                    }
                }
            }
            else {
                sendToClient(ssl, std::string(CMD_ERROR) + "|User not in group");
            }
        }
        else {
            sendToClient(ssl, std::string(CMD_ERROR) + "|Only admin can delete members");
        }
        stateMtx.unlock();
    }
    else if (cmd == CMD_GROUP_DEL) {
        std::string group_name;
        std::getline(iss, group_name);

        stateMtx.lock();
        std::string sender = clients[client_sock].username;
        if (groups.find(group_name) != groups.end() && groups[group_name].admin == sender) {
            for (const auto& member : groups[group_name].members) {
                for (auto& c : clients) {
                    if (c.second.username == member) {
                        sendToClient(c.second.ssl, std::string(CMD_GROUP_NOTIFY) + "|DELETED|" + group_name + "|" + sender + "|*");
                    }
                }
            }
            groups.erase(group_name);
            saveGroupsConfig();
            fs::remove(getGroupFilename(group_name));

            sendToClient(ssl, std::string(CMD_OK) + "|Group deleted.");
            logMessage(SERVER_LOG, "GROUP", sender + " deleted group " + group_name);
        }
        else {
            sendToClient(ssl, std::string(CMD_ERROR) + "|Only admin can delete the group");
        }
        stateMtx.unlock();
    }
}

void handleClient(SOCKET client_sock) {
    SSL* ssl = SSL_new(server_ctx);
    SSL_set_fd(ssl, static_cast<int>(client_sock));
    if (SSL_accept(ssl) <= 0) {
        SSL_free(ssl);
        closesocket(client_sock);
        return;
    }

    stateMtx.lock();
    clients[client_sock] = { "", ssl };
    stateMtx.unlock();

    char buffer[BUFFER_SIZE];
    std::string stream_buffer = "";

    while (server_running) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = SSL_read(ssl, buffer, BUFFER_SIZE - 1);
        if (bytes <= 0) {
            stateMtx.lock();
            if (clients.find(client_sock) != clients.end()) {
                std::string name = clients[client_sock].username;
                clients.erase(client_sock);
                if (!name.empty()) {
                    logMessage(SERVER_LOG, "INFO", "User " + name + " disconnected");
                    std::cout << "[Server] User " << name << " disconnected" << std::endl;
                }
            }
            stateMtx.unlock();
            SSL_shutdown(ssl);
            SSL_free(ssl);
            closesocket(client_sock);
            break;
        }

        stream_buffer += std::string(buffer, bytes);
        size_t pos;
        while ((pos = stream_buffer.find('\n')) != std::string::npos) {
            std::string command_line = stream_buffer.substr(0, pos);
            stream_buffer.erase(0, pos + 1);

            command_line.erase(std::remove(command_line.begin(), command_line.end(), '\r'), command_line.end());
            if (!command_line.empty()) {
                processClientCommand(client_sock, ssl, command_line);
            }
        }
    }
}

int main() {
    // Автоматическое создание всех необходимых директорий при первом запуске
    if (!fs::exists("logs")) fs::create_directory("logs");
    if (!fs::exists("chats")) fs::create_directory("chats");
    if (!fs::exists("groups")) fs::create_directory("groups");

    if (!initNetwork()) {
        std::cerr << "Failed to init network architecture." << std::endl;
        return 1;
    }

    signal(SIGINT, handle_sigint);
#ifndef _WIN32
    signal(SIGHUP, handle_sighup);
#endif

    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    server_ctx = SSL_CTX_new(TLS_server_method());
    if (!server_ctx) {
        std::cerr << "Failed to allocate OpenSSL context." << std::endl;
        cleanupNetwork();
        return 1;
    }

    // Восстановленный inline блок настройки сертификатов SSL (без внешних функций)
    if (SSL_CTX_use_certificate_file(server_ctx, "server.crt", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(server_ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
        std::cerr << "SSL Certificates error." << std::endl;
        SSL_CTX_free(server_ctx);
        cleanupNetwork();
        return 1;
    }

    loadGroupsConfig();

    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        std::cerr << "Socket allocation failed" << std::endl;
        SSL_CTX_free(server_ctx);
        cleanupNetwork();
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(DEFAULT_PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        closesocket(server_fd);
        SSL_CTX_free(server_ctx);
        cleanupNetwork();
        return 1;
    }
    if (listen(server_fd, 10) < 0) {
        std::cerr << "Listen failed" << std::endl;
        closesocket(server_fd);
        SSL_CTX_free(server_ctx);
        cleanupNetwork();
        return 1;
    }

    // Исправлено логирование с использованием std::to_string
    logMessage(SERVER_LOG, "INFO", "Server started on port " + std::to_string(DEFAULT_PORT));
    std::cout << "[Server] Secure TLS Listening on port " << DEFAULT_PORT << std::endl;

    while (server_running) {
        SOCKET client_sock = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_sock == INVALID_SOCKET) {
            if (reload_config) {
                stateMtx.lock();
                loadGroupsConfig();
                stateMtx.unlock();
                reload_config = 0;
            }
            if (!server_running) break;
            continue;
        }

        if (reload_config) {
            stateMtx.lock();
            loadGroupsConfig();
            stateMtx.unlock();
            reload_config = 0;
        }

        std::thread(handleClient, client_sock).detach();
    }

    stateMtx.lock();
    logMessage(SERVER_LOG, "INFO", "Broadcasting termination notification sequence.");
    for (auto& p : clients) {
        sendToClient(p.second.ssl, std::string(CMD_ERROR) + "|Server is down");
        SSL_shutdown(p.second.ssl);
        SSL_free(p.second.ssl);
        closesocket(p.first);
    }
    clients.clear();
    stateMtx.unlock();

    SSL_CTX_free(server_ctx);
    cleanupNetwork();
    logMessage(SERVER_LOG, "INFO", "Server stopped gracefully");
    std::cout << "\n[Server] Stopped gracefully." << std::endl;
    return 0;
}