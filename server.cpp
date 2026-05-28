#include <iostream>
#include <string>
#include <cstring>
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

#include "common.h"
#include "logger.h"

const std::string SERVER_LOG = "server.log";
std::map<int, std::string> clients;

struct Group {
    std::string admin;
    std::set<std::string> members;
};
std::map<std::string, Group> groups;

std::mutex stateMtx;
volatile sig_atomic_t server_running = 1;
int server_fd = -1;

void handle_sigint(int) {
    server_running = 0;
    if (server_fd != -1) {
        close(server_fd);
    }
}

bool sendToClient(int sock, const std::string& message) {
    std::string msg = message + "\n";
    int sent = send(sock, msg.c_str(), msg.length(), 0);
    return sent > 0;
}

void* handleClient(void* arg) {
    int client_sock = *(int*)arg;
    delete (int*)arg;

    char buffer[BUFFER_SIZE];
    while (server_running) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) {
            stateMtx.lock();
            if (clients.find(client_sock) != clients.end()) {
                std::string name = clients[client_sock];
                clients.erase(client_sock);
                logMessage(SERVER_LOG, "INFO", "User " + name + " disconnected");
                std::cout << "[Server] User " << name << " disconnected" << std::endl;
            }
            stateMtx.unlock();
            close(client_sock);
            break;
        }

        std::string data(buffer);
        data.erase(std::remove(data.begin(), data.end(), '\n'), data.end());
        data.erase(std::remove(data.begin(), data.end(), '\r'), data.end());

        std::istringstream iss(data);
        std::string cmd;
        std::getline(iss, cmd, '|');

        if (cmd == CMD_LOGIN) {
            std::string name;
            std::getline(iss, name);

            stateMtx.lock();
            bool nameExists = false;
            for (auto& p : clients) {
                if (p.second == name) {
                    nameExists = true;
                    break;
                }
            }
            if (nameExists) {
                sendToClient(client_sock, CMD_ERROR + "|Username already taken");
                logMessage(SERVER_LOG, "WARNING", "Failed login attempt with name " + name);
            }
            else {
                clients[client_sock] = name;
                sendToClient(client_sock, CMD_OK + "|Logged in as " + name);
                logMessage(SERVER_LOG, "INFO", "User " + name + " connected");
                std::cout << "[Server] User " << name << " connected" << std::endl;
            }
            stateMtx.unlock();
        }
        else if (cmd == CMD_MSG) {
            std::string target, msg;
            std::getline(iss, target, '|');
            std::getline(iss, msg);

            stateMtx.lock();
            std::string sender = clients[client_sock];
            int target_sock = -1;
            for (auto& p : clients) {
                if (p.second == target) {
                    target_sock = p.first;
                    break;
                }
            }
            if (target_sock == -1) {
                sendToClient(client_sock, CMD_ERROR + "|User " + target + " not found");
            }
            else {
                sendToClient(target_sock, CMD_INMSG + "|" + sender + "|" + msg);
                logMessage(SERVER_LOG, "MSG", "Private: " + sender + " -> " + target + ": " + msg);
                sendToClient(client_sock, CMD_OK + "|Message sent");
            }
            stateMtx.unlock();
        }
        else if (cmd == CMD_GROUP_JOIN) {
            std::string group_name;
            std::getline(iss, group_name);

            stateMtx.lock();
            std::string sender = clients[client_sock];
            if (groups.find(group_name) == groups.end()) {
                // Если группы нет — создаем её, автор автоматически становится админом
                Group g;
                g.admin = sender;
                g.members.insert(sender);
                groups[group_name] = g;
                sendToClient(client_sock, CMD_OK + "|Group created. You are admin.");
                logMessage(SERVER_LOG, "GROUP", sender + " created group " + group_name);
            }
            else {
                // ИСПРАВЛЕНО: Если группа существует, войти можно ТОЛЬКО если тебя добавил админ
                if (groups[group_name].members.count(sender)) {
                    sendToClient(client_sock, CMD_OK + "|Joined group.");
                    logMessage(SERVER_LOG, "GROUP", sender + " entered group " + group_name);
                }
                else {
                    sendToClient(client_sock, CMD_ERROR + "|Access denied: You are not a member of this group. Admin must add you.");
                    logMessage(SERVER_LOG, "GROUP", sender + " tried unauthorized join to " + group_name);
                }
            }
            stateMtx.unlock();
        }
        else if (cmd == CMD_GROUP_MSG) {
            std::string group_name, msg;
            std::getline(iss, group_name, '|');
            std::getline(iss, msg);

            stateMtx.lock();
            std::string sender = clients[client_sock];
            if (groups.find(group_name) != groups.end() && groups[group_name].members.count(sender)) {
                for (const auto& member : groups[group_name].members) {
                    for (auto& c : clients) {
                        if (c.second == member && c.first != client_sock) {
                            sendToClient(c.first, CMD_GROUP_MSG + "|" + group_name + "|" + sender + "|" + msg);
                        }
                    }
                }
                logMessage(SERVER_LOG, "GMSG", "[" + group_name + "] " + sender + ": " + msg);
            }
            else {
                sendToClient(client_sock, CMD_ERROR + "|Access denied or group not found");
            }
            stateMtx.unlock();
        }
        else if (cmd == CMD_GROUP_ADD) {
            std::string group_name, target;
            std::getline(iss, group_name, '|');
            std::getline(iss, target);

            stateMtx.lock();
            std::string sender = clients[client_sock];
            if (groups.find(group_name) != groups.end() && groups[group_name].admin == sender) {
                groups[group_name].members.insert(target);
                sendToClient(client_sock, CMD_OK + "|User " + target + " added.");
                logMessage(SERVER_LOG, "GROUP", sender + " added " + target + " to " + group_name);

                for (auto& c : clients) {
                    if (c.second == target) {
                        sendToClient(c.first, CMD_GROUP_NOTIFY + "|ADDED|" + group_name + "|" + sender);
                        break;
                    }
                }
            }
            else {
                sendToClient(client_sock, CMD_ERROR + "|Only admin can add members");
            }
            stateMtx.unlock();
        }
        else if (cmd == CMD_GROUP_KICK) {
            std::string group_name, target;
            std::getline(iss, group_name, '|');
            std::getline(iss, target);

            stateMtx.lock();
            std::string sender = clients[client_sock];
            if (groups.find(group_name) != groups.end() && groups[group_name].admin == sender) {
                if (groups[group_name].members.erase(target)) {
                    sendToClient(client_sock, CMD_OK + "|User " + target + " deleted.");
                    logMessage(SERVER_LOG, "GROUP", sender + " kicked " + target + " from " + group_name);

                    for (auto& c : clients) {
                        if (c.second == target) {
                            sendToClient(c.first, CMD_GROUP_NOTIFY + "|KICKED|" + group_name);
                            break;
                        }
                    }
                }
                else {
                    sendToClient(client_sock, CMD_ERROR + "|User not in group");
                }
            }
            else {
                sendToClient(client_sock, CMD_ERROR + "|Only admin can delete members");
            }
            stateMtx.unlock();
        }
        else if (cmd == CMD_GROUP_DEL) {
            std::string group_name;
            std::getline(iss, group_name);

            stateMtx.lock();
            std::string sender = clients[client_sock];
            if (groups.find(group_name) != groups.end() && groups[group_name].admin == sender) {
                for (const auto& member : groups[group_name].members) {
                    for (auto& c : clients) {
                        if (c.second == member) {
                            sendToClient(c.first, CMD_GROUP_NOTIFY + "|DELETED|" + group_name);
                        }
                    }
                }
                groups.erase(group_name);
                sendToClient(client_sock, CMD_OK + "|Group deleted.");
                logMessage(SERVER_LOG, "GROUP", sender + " deleted group " + group_name);
            }
            else {
                sendToClient(client_sock, CMD_ERROR + "|Only admin can delete the group");
            }
            stateMtx.unlock();
        }
        else if (cmd == CMD_QUIT) {
            stateMtx.lock();
            if (clients.find(client_sock) != clients.end()) {
                std::string name = clients[client_sock];
                logMessage(SERVER_LOG, "INFO", "User " + name + " quit");
                std::cout << "[Server] User " << name << " quit" << std::endl;
                sendToClient(client_sock, CMD_OK + "|Goodbye, " + name);
                clients.erase(client_sock);
            }
            stateMtx.unlock();
            close(client_sock);
            break;
        }
    }
    return nullptr;
}

int main() {
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

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

    logMessage(SERVER_LOG, "INFO", "Server started on port " + std::to_string(DEFAULT_PORT));
    std::cout << "[Server] Listening on port " << DEFAULT_PORT << " (Press Ctrl+C to shutdown)" << std::endl;

    while (server_running) {
        int client_sock = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_sock < 0) {
            if (!server_running) break;
            perror("accept");
            continue;
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

    logMessage(SERVER_LOG, "INFO", "Server stopped gracefully");
    std::cout << "\n[Server] Stopped gracefully." << std::endl;
    return 0;
}