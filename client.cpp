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
#include <readline/readline.h>
#include <readline/history.h>

#include "common.h"
#include "logger.h"

std::string CLIENT_LOG;
int sock = 0;
std::atomic<bool> running(true);

#define COLOR_YELLOW  "\033[33m"
#define COLOR_RED     "\033[31m"
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
            // Очищаем текущую строку ввода перед выводом системного сообщения
            std::cout << "\r\033[K" << COLOR_RED << "\n[Disconnected from server]" << COLOR_RESET << std::endl;
            running = false;
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
            output_to_print = "[" + from + "]: " + msg;
            logMessage(CLIENT_LOG, "IN", "From " + from + ": " + msg);
            need_display_update = true;
        }
        else if (cmd == CMD_OK) {
            std::string info;
            std::getline(iss, info);
            // Если это просто подтверждение отправки, на экран его не выводим, чтобы не спамить
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

        // Если нужно вывести текст на экран, делаем это атомарно для Readline
        if (need_display_update) {
            // \r - возвращает курсор в начало строки, \033[K - очищает строку до конца
            std::cout << "\r\033[K" << output_to_print << std::endl;

            // Магия Readline: уведомляем, что мы перешли на новую строку, 
            // и принудительно перерисовываем prompt вместе с тем, что пользователь успел набрать
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

    // Цикл ввода IP
    while (true) {
        char* input = readline("Enter server IP (or 'localhost'): ");
        if (!input) return 0;
        server_ip = input;
        free(input);
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

    // Создание сокета
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

    // Цикл логина
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

    // Запускаем поток приёма
    pthread_t recv_thread;
    pthread_create(&recv_thread, NULL, receiveThread, NULL);

    // Инициализация истории стрелочек (вверх/вниз)
    using_history();

    // Формируем постоянный prompt для текущего пользователя
    std::string prompt = "[" + username + "]: ";
    char* line;

    while (running) {
        line = readline(prompt.c_str()); // Передаем промпт прямо сюда!
        if (!line) break;
        std::string input(line);
        free(line);
        if (input.empty()) continue;

        // Добавляем команду в историю стрелочек вверх/вниз
        add_history(input.c_str());

        if (input == "/quit") {
            sendCommand(CMD_QUIT);
            running = false;
            break;
        }

        size_t space = input.find(' ');
        if (space == std::string::npos) {
            // \033[A - поднимает курсор вверх на одну строку (туда, где остался сырой неверный ввод)
            // \r\033[K - затирает её и выводит предупреждение
            std::cout << "\033[A\r\033[K" << COLOR_YELLOW << "Usage: <recipient> <message>" << COLOR_RESET << std::endl;
            continue;
        }

        std::string recipient = input.substr(0, space);
        std::string message = input.substr(space + 1);
        if (recipient.empty() || message.empty()) {
            std::cout << "\033[A\r\033[K" << COLOR_YELLOW << "Invalid format. Use: recipient message" << COLOR_RESET << std::endl;
            continue;
        }

        // Элегантный трюк: стираем сырую строчку readline (например, "Alexey привет")
        // и на ее месте пишем красивое "[Sveta]: привет"
        std::cout << "\033[A\r\033[K[" << username << "]: " << message << std::endl;

        std::string cmd = CMD_MSG + "|" + recipient + "|" + message;
        if (!sendCommand(cmd)) {
            std::cout << COLOR_RED << "Failed to send message. Connection lost?" << COLOR_RESET << std::endl;
            running = false;
            break;
        }
        logMessage(CLIENT_LOG, "OUT", "To " + recipient + ": " + message);
    }

    close(sock);
    pthread_join(recv_thread, NULL);
    logMessage(CLIENT_LOG, "INFO", "Client terminated");
    return 0;
}