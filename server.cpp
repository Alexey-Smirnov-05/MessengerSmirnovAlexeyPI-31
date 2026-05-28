#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>
#include <pthread.h>
#include <signal.h>
#include <mutex>
#include <errno.h>

// Заголовочные файлы OpenSSL
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "common.h"
#include "logger.h"

const std::string SERVER_LOG = "server.log";
const std::string GROUPS_CONFIG_FILE = "groups_config.txt";

struct ClientInfo {
    std::string username;
    SSL* ssl;
};

// Хранилище подключенных клиентов: сокет -> структура ClientInfo
std::map<int, ClientInfo> clients;

struct Group {
    std::string admin;
    std::set<std::string> members;
};
std::map<std::string, Group> groups;

std::mutex stateMtx;
volatile sig_atomic_t server_running = 1;
volatile sig_atomic_t reload_config = 0;
int server_fd = -1;
SSL_CTX* server_ctx = nullptr;

// Обработчик сигнала Ctrl+C (SIGINT) для корректного закрытия сервера
void handle_sigint(int) {
    server_running = 0;
    if (server_fd != -1) {
        close(server_fd);
    }
}

// Обработчик сигнала SIGHUP для перечитывания конфига на лету
void handle_sighup(int) {
    reload_config = 1;
}

std::string getPMFilename(std::string u1, std::string u2) {
    if (u1 > u2) std::swap(u1, u2);
    return "history_pm_" + u1 + "_" + u2 + ".txt";
}

std::string getGroupFilename(std::string gname) {
    if (!gname.empty() && gname[0] == '#') {
        return "history_group_" + gname.substr(1) + ".txt";
    }
    return "history_group_" + gname + ".txt";
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

// Безопасная отправка данных через TLS-слой OpenSSL
bool sendToClient(SSL* ssl, const std::string& message) {
    if (!ssl) return false;
    std::string msg = message + "\n";
    int sent = SSL_write(ssl, msg.c_str(), msg.length());
    return sent > 0;
}

void sendPMHistoryToClient(SSL* ssl, const std::string& u1, const std::string& u2) {
    std::ifstream f(getPMFilename(u1, u2));
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) sendToClient(ssl, CMD_HIST_LINE + "|PM|" + line);
    }
}

void sendGroupHistoryToClient(SSL* ssl, const std::string& gname) {
    std::ifstream f(getGroupFilename(gname));
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) sendToClient(ssl, CMD_HIST_LINE + "|GROUP|" + gname + "|" + line);
    }
}

void processClientCommand(int client_sock, SSL* ssl, const std::string& data) {
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
            sendToClient(ssl, CMD_ERROR + "|Username already taken");
        }
        else {
            clients[client_sock].username = name;
            sendToClient(ssl, CMD_OK + "|Logged in as " + name);
            logMessage(SERVER_LOG, "INFO", "User " + name + " connected securely");
            std::cout << "[Server] User " << name << " connected securely" << std::endl;
        }
        stateMtx.unlock();
    }
    else if (cmd == CMD_MSG) {
        std::string target, msg;
        std::getline(iss, target, '|');
        std::getline(iss, msg);

        stateMtx.lock();
        std::string sender = clients[client_sock].username;

        appendPMHistory(sender, target, sender, msg);
        logMessage(SERVER_LOG, "MSG", "Private: " + sender + " -> " + target + ": " + msg);

        SSL* target_ssl = nullptr;
        for (auto& p : clients) {
            if (p.second.username == target) {
                target_ssl = p.second.ssl;
                break;
            }
        }
        if (target_ssl == nullptr) {
            sendToClient(ssl, CMD_OK + "|Message saved (User is offline)");
        }
        else {
            sendToClient(target_ssl, CMD_INMSG + "|" + sender + "|" + msg);
            sendToClient(ssl, CMD_OK + "|Message sent");
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

            sendToClient(ssl, CMD_OK + "|Group created. You are admin.");
            logMessage(SERVER_LOG, "GROUP", sender + " created group " + group_name);
            sendGroupHistoryToClient(ssl, group_name);
        }
        else {
            if (groups[group_name].members.count(sender)) {
                sendToClient(ssl, CMD_OK + "|Joined group.");
                logMessage(SERVER_LOG, "GROUP", sender + " entered group " + group_name);
                sendGroupHistoryToClient(ssl, group_name);
            }
            else {
                sendToClient(ssl, CMD_ERROR + "|Access denied: You are not a member of this group. Admin must add you.");
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
            appendGroupHistory(group_name, sender, msg);
            logMessage(SERVER_LOG, "GMSG", "[" + group_name + "] " + sender + ": " + msg);

            for (const auto& member : groups[group_name].members) {
                for (auto& c : clients) {
                    if (c.second.username == member && c.first != client_sock) {
                        sendToClient(c.second.ssl, CMD_GROUP_MSG + "|" + group_name + "|" + sender + "|" + msg);
                    }
                }
            }
        }
        else {
            sendToClient(ssl, CMD_ERROR + "|Access denied or group not found");
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

            sendToClient(ssl, CMD_OK + "|User " + target + " added.");
            logMessage(SERVER_LOG, "GROUP", sender + " added " + target + " to " + group_name);

            for (auto& c : clients) {
                if (c.second.username == target) {
                    sendToClient(c.second.ssl, CMD_GROUP_NOTIFY + "|ADDED|" + group_name + "|" + sender);
                    break;
                }
            }
        }
        else {
            sendToClient(ssl, CMD_ERROR + "|Only admin can add members");
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

                sendToClient(ssl, CMD_OK + "|User " + target + " deleted.");
                logMessage(SERVER_LOG, "GROUP", sender + " kicked " + target + " from " + group_name);

                for (auto& c : clients) {
                    if (c.second.username == target) {
                        sendToClient(c.second.ssl, CMD_GROUP_NOTIFY + "|KICKED|" + group_name + "|" + sender);
                        break;
                    }
                }
            }
            else {
                sendToClient(ssl, CMD_ERROR + "|User not in group");
            }
        }
        else {
            sendToClient(ssl, CMD_ERROR + "|Only admin can delete members");
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
                        sendToClient(c.second.ssl, CMD_GROUP_NOTIFY + "|DELETED|" + group_name);
                    }
                }
            }
            groups.erase(group_name);
            saveGroupsConfig();
            std::remove(getGroupFilename(group_name).c_str());

            sendToClient(ssl, CMD_OK + "|Group deleted.");
            logMessage(SERVER_LOG, "GROUP", sender + " deleted group " + group_name);
        }
        else {
            sendToClient(ssl, CMD_ERROR + "|Only admin can delete the group");
        }
        stateMtx.unlock();
    }
}

void* handleClient(void* arg) {
    int client_sock = *(int*)arg;
    delete (int*)arg;

    // Обертка системного сокета в защищенную TLS-сессию OpenSSL
    SSL* ssl = SSL_new(server_ctx);
    SSL_set_fd(ssl, client_sock);
    if (SSL_accept(ssl) <= 0) {
        SSL_free(ssl);
        close(client_sock);
        return nullptr;
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
            close(client_sock);
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
    return nullptr;
}

int main() {
    // Регистрация обработчика SIGINT (Ctrl+C) для мягкой остановки
    struct sigaction sa_int;
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    // Регистрация обработчика SIGHUP для перезагрузки конфигурации
    struct sigaction sa_hup;
    sa_hup.sa_handler = handle_sighup;
    sigemptyset(&sa_hup.sa_mask);
    sa_hup.sa_flags = 0;
    sigaction(SIGHUP, &sa_hup, NULL);

    // Инициализация криптографического движка OpenSSL
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    server_ctx = SSL_CTX_new(TLS_server_method());
    if (!server_ctx) {
        std::cerr << "Failed to allocate OpenSSL TLS memory engine context." << std::endl;
        return 1;
    }

    // Привязка SSL-сертификата и приватного ключа
    if (SSL_CTX_use_certificate_file(server_ctx, "server.crt", SSL_FILETYPE_PEM) <= 0 ||
        SSL_CTX_use_PrivateKey_file(server_ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
        std::cerr << "SSL Certificates mismatch. Generate server.crt and server.key first!" << std::endl;
        return 1;
    }

    loadGroupsConfig();

    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(DEFAULT_PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    logMessage(SERVER_LOG, "INFO", "Secure Server started on port " + std::to_string(DEFAULT_PORT));
    std::cout << "[Server] Secure TLS Listening on port " << DEFAULT_PORT << " (PID: " << getpid() << ")" << std::endl;

    while (server_running) {
        int client_sock = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_sock < 0) {
            if (reload_config) {
                stateMtx.lock();
                loadGroupsConfig(); // Перечитывание конфигурационных файлов «на лету»
                stateMtx.unlock();
                reload_config = 0;
            }
            if (!server_running) break;
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        if (reload_config) {
            stateMtx.lock();
            loadGroupsConfig();
            stateMtx.unlock();
            reload_config = 0;
        }

        int* new_sock = new int;
        *new_sock = client_sock;
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handleClient, new_sock) != 0) {
            delete new_sock;
            close(client_sock);
        }
        else {
            pthread_detach(thread_id);
        }
    }

    // Рассылка уведомлений клиентам при закрытии сервера: «Сервер пал, милорд»
    stateMtx.lock();
    logMessage(SERVER_LOG, "INFO", "Broadcasting termination notification sequence.");
    for (auto& p : clients) {
        sendToClient(p.second.ssl, CMD_ERROR + "|Server is down, milord!");
        SSL_shutdown(p.second.ssl);
        SSL_free(p.second.ssl);
        close(p.first);
    }
    clients.clear();
    stateMtx.unlock();

    SSL_CTX_free(server_ctx);
    logMessage(SERVER_LOG, "INFO", "Server stopped gracefully");
    std::cout << "\n[Server] Stopped gracefully." << std::endl;
    return 0;
}