#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <sstream>
#include <algorithm>
#include <pthread.h>
#include <signal.h>

#include "common.h"
#include "logger.h"

const std::string SERVER_LOG = "server.log";
std::map<int, std::string> clients;
volatile sig_atomic_t server_running = 1;
int server_fd = -1; // Вынесли в глобальную область, чтобы закрыть при Ctrl+C

void handle_sigint(int) {
    server_running = 0;
    if (server_fd != -1) {
        // Насильно закрываем слушающий сокет, чтобы прервать блокирующий accept()
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
            if (clients.find(client_sock) != clients.end()) {
                std::string name = clients[client_sock];
                clients.erase(client_sock);
                logMessage(SERVER_LOG, "INFO", "User " + name + " disconnected");
                std::cout << "[Server] User " << name << " disconnected" << std::endl;
            }
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
        }
        else if (cmd == CMD_MSG) {
            std::string target, msg;
            std::getline(iss, target, '|');
            std::getline(iss, msg);
            if (clients.find(client_sock) == clients.end()) {
                sendToClient(client_sock, CMD_ERROR + "|Not logged in");
                continue;
            }
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
                logMessage(SERVER_LOG, "WARNING", "Message from " + sender + " to " + target + " failed: user not found");
            }
            else {
                sendToClient(target_sock, CMD_INMSG + "|" + sender + "|" + msg);
                logMessage(SERVER_LOG, "INFO", "Message from " + sender + " to " + target + ": " + msg);
                sendToClient(client_sock, CMD_OK + "|Message sent");
            }
        }
        else if (cmd == CMD_QUIT) {
            if (clients.find(client_sock) != clients.end()) {
                std::string name = clients[client_sock];
                logMessage(SERVER_LOG, "INFO", "User " + name + " quit");
                std::cout << "[Server] User " << name << " quit" << std::endl;
                sendToClient(client_sock, CMD_OK + "|Goodbye, " + name);
                clients.erase(client_sock);
            }
            else {
                sendToClient(client_sock, CMD_OK + "|Goodbye");
            }
            close(client_sock);
            break;
        }
    }
    return nullptr;
}

int main() {
    // Настраиваем обработку Ctrl+C
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
        // Добавляем int перед client_sock
        int client_sock = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);
        if (client_sock < 0) {
            // Если accept завершился ошибкой из-за закрытия сокета по Ctrl+C
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