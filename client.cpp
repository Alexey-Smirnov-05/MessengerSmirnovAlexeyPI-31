#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sstream>
#include <atomic>
#include <mutex>
#include <readline/readline.h>
#include <readline/history.h>

#include "common.h"
#include "logger.h"

std::string CLIENT_LOG;
int sock = 0;
std::atomic<bool> running(true);

std::string active_chat_partner = "";
std::mutex stateMutex;

#define COLOR_YELLOW  "\033[33m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RESET   "\033[0m"

bool sendCommand(const std::string& cmd) {
    std::string msg = cmd + "\n";
    int sent = send(sock, msg.c_str(), msg.length(), 0);
    return sent > 0;
}

void* receiveThread(void*) {
    char buffer[BUFFER_SIZE];
    while (running) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) {
            if (running) { // Выводим ошибку только если мы не сами выходим
                std::cout << "\r\033[K" << COLOR_RED << "\n[Disconnected from server]" << COLOR_RESET << std::endl;
                running = false;
            }
            break;
        }

        std::string data(buffer);
        std::istringstream iss(data);
        std::string cmd;
        std::getline(iss, cmd, '|');

        bool need_display_update = false;
        std::string output_to_print = "";

        if (cmd == CMD_INMSG) {
            std::string from, msg;
            std::getline(iss, from, '|');
            std::getline(iss, msg);

            stateMutex.lock();
            std::string current_partner = active_chat_partner;
            stateMutex.unlock();

            if (from == current_partner) {
                output_to_print = "[" + from + "]: " + msg;
            }
            else {
                output_to_print = std::string(COLOR_GREEN) + "[Уведомление]: Новое сообщение от " + from + ": " + msg + COLOR_RESET;
            }
            logMessage(CLIENT_LOG, "IN", "From " + from + ": " + msg);
            need_display_update = true;
        }
        else if (cmd == CMD_OK) {
            std::string info;
            std::getline(iss, info);
            // Если это ответ на логаут, глушим вывод, чтобы не дублировать на выходе
            if (info.find("Goodbye") != std::string::npos) {
                continue;
            }
            if (info != "Message sent") {
                output_to_print = COLOR_YELLOW + std::string("[Server]: ") + info + COLOR_RESET;
                need_display_update = true;
            }
            logMessage(CLIENT_LOG, "INFO", info);
        }
        else if (cmd == CMD_ERROR) {
            std::string err;
            std::getline(iss, err);
            output_to_print = COLOR_RED + std::string("[Error]: ") + err + COLOR_RESET;
            need_display_update = true;
        }

        if (need_display_update && running) {
            std::cout << "\r\033[K" << output_to_print << std::endl;
            rl_on_new_line();
            rl_redisplay();
        }
    }
    return nullptr;
}

int main() {
    std::string server_ip;
    int port = DEFAULT_PORT;
    std::string username;

    while (true) {
        char* input_ip = readline("Enter server IP (or 'localhost'): ");
        if (!input_ip) return 0;
        server_ip = input_ip;
        free(input_ip);
        if (server_ip == "localhost" || server_ip == "127.0.0.1") {
            server_ip = "127.0.0.1";
            break;
        }
        struct sockaddr_in test;
        if (inet_pton(AF_INET, server_ip.c_str(), &test.sin_addr) == 1) {
            break;
        }
        std::cout << COLOR_RED << "[Error]: Invalid IP address, please try again." << COLOR_RESET << std::endl;
    }

    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return 1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        return 1;
    }
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }

    bool logged_in = false;
    while (!logged_in) {
        char* name_input = readline("Enter your username: ");
        if (!name_input) break;
        username = name_input;
        free(name_input);
        if (username.empty()) continue;

        CLIENT_LOG = "client_" + username + ".log";
        logMessage(CLIENT_LOG, "INFO", "Attempting to connect as " + username);

        if (!sendCommand(CMD_LOGIN + "|" + username)) {
            std::cerr << "Failed to send login" << std::endl;
            close(sock);
            return 1;
        }

        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) {
            std::cerr << "Server disconnected during login" << std::endl;
            close(sock);
            return 1;
        }

        std::string response(buffer);
        if (response.find(CMD_OK) == 0 && response.find("Logged in as") != std::string::npos) {
            std::cout << COLOR_YELLOW << "[Server]: Logged in as " << username << COLOR_RESET << std::endl;
            logged_in = true;
            logMessage(CLIENT_LOG, "INFO", "Login successful");
        }
        else if (response.find(CMD_ERROR) == 0 && response.find("Username already taken") != std::string::npos) {
            std::cout << COLOR_RED << "[Error]: Username already taken, please choose another." << COLOR_RESET << std::endl;
        }
        else {
            std::cout << COLOR_RED << "[Error]: Login failed: " << response << COLOR_RESET << std::endl;
            close(sock);
            return 1;
        }
    }

    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receiveThread, NULL);

    using_history();
    std::string prompt = "[" + username + "]: ";
    char* line;

    while (running) {
        line = readline(prompt.c_str());
        if (!line) break;
        std::string input(line);
        free(line);
        if (input.empty()) continue;

        add_history(input.c_str());

        if (input == "/quit") {
            running = false; // Отключаем флаг отрисовщика немедленно
            sendCommand(CMD_QUIT);
            break;
        }

        stateMutex.lock();
        std::string current_partner = active_chat_partner;
        stateMutex.unlock();

        if (!current_partner.empty()) {
            if (input == "/exit") {
                std::cout << "\033[A\r\033[K" << COLOR_YELLOW << "[Вы вышли из чата с " << current_partner << "]" << COLOR_RESET << std::endl;
                // Твоя разделительная черта завершения чата
                std::cout << "========================================" << std::endl;

                stateMutex.lock();
                active_chat_partner = "";
                stateMutex.unlock();
                continue;
            }
            if (input.rfind("/chat ", 0) == 0) {
                std::cout << "\033[A\r\033[K" << COLOR_RED << "[Ошибка]: Вы уже в чате с " << current_partner << ". Сначала введите /exit" << COLOR_RESET << std::endl;
                continue;
            }

            std::cout << "\033[A\r\033[K[" << username << "]: " << input << std::endl;

            std::string cmd = CMD_MSG + "|" + current_partner + "|" + input;
            if (!sendCommand(cmd)) {
                std::cout << COLOR_RED << "Failed to send message." << COLOR_RESET << std::endl;
                running = false;
                break;
            }
            logMessage(CLIENT_LOG, "OUT", "To " + current_partner + ": " + input);
        }
        else {
            if (input == "/exit") {
                std::cout << "\033[A\r\033[K" << COLOR_RED << "[Ошибка]: Вы не находитесь в чате." << COLOR_RESET << std::endl;
                continue;
            }
            if (input.rfind("/chat ", 0) == 0) {
                std::string target = input.substr(6);
                target.erase(target.find_last_not_of(" \t\n\r\f\v") + 1);
                target.erase(0, target.find_first_not_of(" \t\n\r\f\v"));

                if (target.empty() || target == username) {
                    std::cout << "\033[A\r\033[K" << COLOR_RED << "[Ошибка]: Некорректное имя пользователя." << COLOR_RESET << std::endl;
                    continue;
                }

                stateMutex.lock();
                active_chat_partner = target;
                stateMutex.unlock();

                // Отрисовка чистой статической рамки без динамического статуса
                std::cout << "\033[A\r\033[K"
                    << "\n========================================\n"
                    << " Чат с: " << target << "\n"
                    << "========================================\n"
                    << "Для выхода введите /exit\n" << std::endl;
                continue;
            }

            std::cout << "\033[A\r\033[K" << COLOR_YELLOW << "Вы не в чате. Используйте: /chat <имя_пользователя>" << COLOR_RESET << std::endl;
        }
    }

    // Чистим за собой состояние терминала readline, чтобы убрать дублирование /quit
    rl_cleanup_after_signal();
    close(sock);
    pthread_join(recv_thread, NULL);
    logMessage(CLIENT_LOG, "INFO", "Client terminated");

    std::cout << COLOR_YELLOW << "[Server]: Goodbye, " << username << COLOR_RESET << std::endl;
    return 0;
}