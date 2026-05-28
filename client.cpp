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

std::string my_username; // ИСПРАВЛЕНО: Переменная стала глобальной для доступа из потока чтения
std::string active_chat_partner = "";
std::string active_group = "";
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
            if (running) {
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
                output_to_print = std::string(COLOR_GREEN) + "[Уведомление]: ЛС от " + from + ": " + msg + COLOR_RESET;
            }
            logMessage(CLIENT_LOG, "IN", "From " + from + ": " + msg);
            need_display_update = true;
        }
        else if (cmd == CMD_GROUP_MSG) {
            std::string gname, from, msg;
            std::getline(iss, gname, '|');
            std::getline(iss, from, '|');
            std::getline(iss, msg);

            stateMutex.lock();
            std::string current_group = active_group;
            stateMutex.unlock();

            if (gname == current_group) {
                output_to_print = "[" + from + "]: " + msg;
            }
            else {
                output_to_print = std::string(COLOR_GREEN) + "[Уведомление]: Новое в " + gname + " от " + from + ": " + msg + COLOR_RESET;
            }
            logMessage(CLIENT_LOG, "G_IN", "[" + gname + "] " + from + ": " + msg);
            need_display_update = true;
        }
        else if (cmd == CMD_GROUP_NOTIFY) {
            std::string type, gname;
            std::getline(iss, type, '|');
            std::getline(iss, gname, '|');

            stateMutex.lock();
            // ИСПРАВЛЕНО: Теперь принудительно и мгновенно меняем промт readline на базовый
            if (type == "KICKED" && active_group == gname) {
                active_group = "";
                output_to_print = std::string(COLOR_RED) + "\n[Вы были удалены из группы " + gname + "]\n========================================" + COLOR_RESET;
                need_display_update = true;

                std::string fallback_prompt = "[" + my_username + "]: ";
                rl_set_prompt(fallback_prompt.c_str());
            }
            else if (type == "DELETED" && active_group == gname) {
                active_group = "";
                output_to_print = std::string(COLOR_RED) + "\n[Группа " + gname + " была удалена администратором]\n========================================" + COLOR_RESET;
                need_display_update = true;

                std::string fallback_prompt = "[" + my_username + "]: ";
                rl_set_prompt(fallback_prompt.c_str());
            }
            else if (type == "ADDED") {
                std::string admin;
                std::getline(iss, admin);
                output_to_print = std::string(COLOR_GREEN) + "[Уведомление]: Администратор " + admin + " добавил вас в группу " + gname + " (Теперь вы можете войти в неё)" + COLOR_RESET;
                need_display_update = true;
            }
            stateMutex.unlock();
        }
        else if (cmd == CMD_OK) {
            std::string info;
            std::getline(iss, info);
            if (info.find("Goodbye") != std::string::npos) continue;

            if (info != "Message sent" && info != "Group message sent" && info != "Joined group.") {
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

            // Если сервер вернул ошибку входа в группу, сбрасываем контекст группы
            stateMutex.lock();
            if (!active_group.empty()) {
                active_group = "";
                std::string fallback_prompt = "[" + my_username + "]: ";
                rl_set_prompt(fallback_prompt.c_str());
            }
            stateMutex.unlock();
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
        if (inet_pton(AF_INET, server_ip.c_str(), &test.sin_addr) == 1) break;
        std::cout << COLOR_RED << "[Error]: Invalid IP address." << COLOR_RESET << std::endl;
    }

    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) return 1;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(DEFAULT_PORT);
    inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }

    bool logged_in = false;
    while (!logged_in) {
        char* name_input = readline("Enter your username: ");
        if (!name_input) break;
        my_username = name_input;
        free(name_input);
        if (my_username.empty()) continue;

        CLIENT_LOG = "client_" + my_username + ".log";
        sendCommand(CMD_LOGIN + "|" + my_username);

        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) {
            close(sock);
            return 1;
        }

        std::string response(buffer);
        if (response.find(CMD_OK) == 0) {
            std::cout << COLOR_YELLOW << "[Server]: Logged in as " << my_username << COLOR_RESET << std::endl;
            logged_in = true;
        }
        else {
            std::cout << COLOR_RED << "[Error]: Login failed or name taken." << COLOR_RESET << std::endl;
        }
    }

    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receiveThread, NULL);
    using_history();

    while (running) {
        std::string prompt;
        stateMutex.lock();
        if (!active_chat_partner.empty()) {
            prompt = "[" + my_username + " -> " + active_chat_partner + "]: ";
        }
        else if (!active_group.empty()) {
            prompt = "[" + my_username + " @ " + active_group + "]: ";
        }
        else {
            prompt = "[" + my_username + "]: ";
        }
        stateMutex.unlock();

        char* line = readline(prompt.c_str());
        if (!line) break;
        std::string input(line);
        free(line);
        if (input.empty()) continue;

        add_history(input.c_str());

        if (input == "/quit") {
            running = false;
            sendCommand(CMD_QUIT);
            break;
        }

        stateMutex.lock();
        std::string current_partner = active_chat_partner;
        std::string current_group = active_group;
        stateMutex.unlock();

        if (!current_partner.empty()) {
            if (input == "/exit") {
                std::cout << "\033[A\r\033[K" << COLOR_YELLOW << "[Вы вышли из чата с " << current_partner << "]" << COLOR_RESET << std::endl;
                std::cout << "========================================" << std::endl;
                stateMutex.lock();
                active_chat_partner = "";
                stateMutex.unlock();
                continue;
            }
            std::cout << "\033[A\r\033[K[" << my_username << "]: " << input << std::endl;
            sendCommand(CMD_MSG + "|" + current_partner + "|" + input);
            logMessage(CLIENT_LOG, "OUT", "To " + current_partner + ": " + input);
        }
        else if (!current_group.empty()) {
            if (input == "/exit") {
                std::cout << "\033[A\r\033[K" << COLOR_YELLOW << "[Вы вышли из контекста группы " << current_group << "]" << COLOR_RESET << std::endl;
                std::cout << "========================================" << std::endl;
                stateMutex.lock();
                active_group = "";
                stateMutex.unlock();
                continue;
            }
            if (input.rfind("/add ", 0) == 0) {
                std::string target = input.substr(5);
                sendCommand(CMD_GROUP_ADD + "|" + current_group + "|" + target);
                continue;
            }
            if (input.rfind("/delete ", 0) == 0) {
                std::string target = input.substr(8);
                sendCommand(CMD_GROUP_KICK + "|" + current_group + "|" + target);
                continue;
            }
            if (input == "/delete_group") {
                sendCommand(CMD_GROUP_DEL + "|" + current_group);
                continue;
            }

            std::cout << "\033[A\r\033[K[" << my_username << "]: " << input << std::endl;
            sendCommand(CMD_GROUP_MSG + "|" + current_group + "|" + input);
            logMessage(CLIENT_LOG, "G_OUT", "[" + current_group + "] " + input);
        }
        else {
            if (input.rfind("/chat ", 0) == 0) {
                std::string target = input.substr(6);
                if (target == my_username) continue;
                stateMutex.lock();
                active_chat_partner = target;
                stateMutex.unlock();
                std::cout << "\033[A\r\033[K\n========================================\n Чат с: " << target << "\n========================================\nДля выхода введите /exit\n" << std::endl;
            }
            else if (input.rfind("/group ", 0) == 0) {
                std::string gname = input.substr(7);
                if (gname.empty() || gname[0] != '#') {
                    std::cout << "\033[A\r\033[K" << COLOR_RED << "[Ошибка]: Имя группы должно начинаться с '#'" << COLOR_RESET << std::endl;
                    continue;
                }
                stateMutex.lock();
                active_group = gname;
                stateMutex.unlock();
                std::cout << "\033[A\r\033[K\n========================================\n Групповой чат: " << gname << "\n========================================\nКоманды админа: /add <имя>, /delete <имя>, /delete_group\nДля выхода введите /exit\n" << std::endl;
                sendCommand(CMD_GROUP_JOIN + "|" + gname);
            }
            else {
                std::cout << "\033[A\r\033[K" << COLOR_YELLOW << "Используйте: /chat <имя> или /group <#название>" << COLOR_RESET << std::endl;
            }
        }
    }

    rl_cleanup_after_signal();
    close(sock);
    pthread_join(recv_thread, NULL);
    std::cout << COLOR_YELLOW << "[Server]: Goodbye, " << my_username << COLOR_RESET << std::endl;
    return 0;
}