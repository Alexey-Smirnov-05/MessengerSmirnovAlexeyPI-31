#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <thread>
#include <chrono>

// --- НАСТРОЙКА READLINE ДЛЯ WINDOWS И LINUX ---
#ifndef _WIN32
#include <readline/readline.h>
#include <readline/history.h>
#else
    // Заглушки функций readline для Windows, чтобы код компилировался без внешних зависимостей
inline char* readline(const char* prompt) {
    std::cout << prompt;
    std::string s;
    if (!std::getline(std::cin, s)) return nullptr;
    char* res = (char*)malloc(s.size() + 1);
#ifdef _MSC_VER
    strcpy_s(res, s.size() + 1, s.c_str());
#else
    strcpy(res, s.c_str());
#endif
    return res;
}
inline void add_history(const char*) {}
inline void using_history() {}
inline void rl_on_new_line() {}
inline void rl_redisplay() {}
inline void rl_cleanup_after_signal() {}
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "common.h"
#include "logger.h"

std::string CLIENT_LOG;
SOCKET sock = INVALID_SOCKET;
SSL* ssl_conn = nullptr;
SSL_CTX* client_ctx = nullptr;
std::atomic<bool> running(true);

std::string my_username;
std::string active_chat_partner = "";
std::string active_group = "";
std::string pending_group = "";

std::string last_msg_sender = "";
std::string last_msg_text = "";

std::mutex stateMutex;
#define COLOR_YELLOW  "\033[33m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_RESET   "\033[0m"

bool sendCommand(const std::string& cmd) {
    if (!ssl_conn) return false;
    std::string msg = cmd + "\n";
    int sent = SSL_write(ssl_conn, msg.c_str(), static_cast<int>(msg.length()));
    return sent > 0;
}

void processIncomingPacket(const std::string& data) {
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
        last_msg_sender = from;
        last_msg_text = msg;
        stateMutex.unlock();
        if (from == current_partner) {
            output_to_print = "[" + from + "]: " + msg;
        }
        else {
            output_to_print = std::string(COLOR_GREEN) + "[Notification]: PM from " + from + ": " + msg + COLOR_RESET;
        }
        logMessage(CLIENT_LOG, "IN", "From " + from + ": " + msg);
        need_display_update = true;
    }
    else if (cmd == CMD_ONLINE_LIST) {
        std::string list_str;
        std::getline(iss, list_str);
        std::istringstream oiss(list_str);
        std::string uname;

        output_to_print = std::string(COLOR_YELLOW) + "--- Users Online ---" + COLOR_RESET;
        while (std::getline(oiss, uname, ',')) {
            if (!uname.empty()) {
                output_to_print += "\n" + uname;
            }
        }
        output_to_print += std::string(COLOR_YELLOW) + "\n--------------------" + COLOR_RESET;
        need_display_update = true;
    }
    else if (cmd == CMD_GROUP_MSG) {
        std::string gname, from, msg;
        std::getline(iss, gname, '|');
        std::getline(iss, from, '|');
        std::getline(iss, msg);

        stateMutex.lock();
        std::string current_group = active_group;
        last_msg_sender = from;
        last_msg_text = msg;
        stateMutex.unlock();
        if (gname == current_group) {
            output_to_print = "[" + from + "]: " + msg;
        }
        else {
            output_to_print = std::string(COLOR_GREEN) + "[Notification]: New in " + gname + " from " + from + ": " + msg + COLOR_RESET;
        }
        logMessage(CLIENT_LOG, "G_IN", "[" + gname + "] " + from + ": " + msg);
        need_display_update = true;
    }
    else if (cmd == CMD_HIST_LINE) {
        std::string type;
        std::getline(iss, type, '|');
        if (type == "PM") {
            std::string from, msg;
            std::getline(iss, from, '|');
            std::getline(iss, msg);
            output_to_print = "[" + from + "]: " + msg;
            need_display_update = true;
        }
        else if (type == "GROUP") {
            std::string gname, from, msg;
            std::getline(iss, gname, '|');
            std::getline(iss, from, '|');
            std::getline(iss, msg);
            output_to_print = "[" + from + "]: " + msg;
            need_display_update = true;
        }
    }
    else if (cmd == CMD_GROUP_NOTIFY) {
        std::string type, gname;
        std::getline(iss, type, '|');
        std::getline(iss, gname, '|');

        stateMutex.lock();
        if (type == "KICKED") {
            std::string admin;
            std::getline(iss, admin);
            if (active_group == gname) {
                active_group = "";
                output_to_print = std::string(COLOR_RED) + "\n[You were removed from group " + gname + " by admin " + admin + "]" + COLOR_RESET + "\n========================================";
            }
            else {
                output_to_print = std::string(COLOR_RED) + "[Notification]: Admin " + admin + " removed you from group " + gname + COLOR_RESET;
            }
            need_display_update = true;
        }
        else if (type == "DELETED") {
            if (active_group == gname) {
                active_group = "";
                output_to_print = std::string(COLOR_RED) + "\n[Group " + gname + " was deleted by admin]" + COLOR_RESET + "\n========================================";
            }
            else {
                output_to_print = std::string(COLOR_RED) + "[Notification]: Group " + gname + " deleted by admin." + COLOR_RESET;
            }
            need_display_update = true;
        }
        else if (type == "ADDED") {
            std::string admin;
            std::getline(iss, admin);
            output_to_print = std::string(COLOR_GREEN) + "[Notification]: Admin " + admin + " added you to group " + gname + " (You can now join it)" + COLOR_RESET;
            need_display_update = true;
        }
        stateMutex.unlock();
    }
    else if (cmd == CMD_OK) {
        std::string info;
        std::getline(iss, info);

        stateMutex.lock();
        if (!pending_group.empty() && (info.find("Group created") != std::string::npos || info.find("Joined group") != std::string::npos)) {
            active_group = pending_group;
            std::string gname = active_group;
            pending_group = "";
            stateMutex.unlock();

            output_to_print = "\n========================================\n Group Chat: " + gname + "\n========================================\nAdmin commands: /add <name>, /delete <name>, /delete_group\nType /exit to leave\n";
            need_display_update = true;
        }
        else {
            stateMutex.unlock();
            if (info != "Message sent" && info != "Group message sent") {
                output_to_print = COLOR_YELLOW + std::string("[Server]: ") + info + COLOR_RESET;
                need_display_update = true;
            }
        }
        logMessage(CLIENT_LOG, "INFO", info);
    }
    else if (cmd == CMD_ERROR) {
        std::string err;
        std::getline(iss, err);
        output_to_print = COLOR_RED + std::string("[Error]: ") + err + COLOR_RESET;
        need_display_update = true;

        stateMutex.lock();
        pending_group = "";
        // ИСПРАВЛЕНО: Больше не сбрасываем active_group = "", чтобы пользователя не выкидывало из чата при ошибках!
        stateMutex.unlock();
    }

    // ИСПРАВЛЕНО: Печатаем ответ сервера всегда, но обновляем readline-промпт только если работа продолжается
    if (need_display_update) {
        std::cout << "\r\033[K" << output_to_print << std::endl;
        if (running) {
            rl_on_new_line();
            rl_redisplay();
        }
    }
}

void receiveThread() {
    char buffer[BUFFER_SIZE];
    std::string stream_buffer = "";
    while (running) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes = SSL_read(ssl_conn, buffer, BUFFER_SIZE - 1);
        if (bytes <= 0) {
            if (running) {
                std::cout << "\r\033[K" << COLOR_RED << "\n[Disconnected from server]" << COLOR_RESET << std::endl;
                running = false;
            }
            break;
        }

        stream_buffer += std::string(buffer, bytes);
        size_t pos;
        while ((pos = stream_buffer.find('\n')) != std::string::npos) {
            std::string packet = stream_buffer.substr(0, pos);
            stream_buffer.erase(0, pos + 1);

            packet.erase(std::remove(packet.begin(), packet.end(), '\r'), packet.end());
            if (!packet.empty()) {
                processIncomingPacket(packet);
            }
        }
    }
}

int main() {
    if (!initNetwork()) {
        std::cerr << "Failed to init network architecture." << std::endl;
        return 1;
    }

    std::string server_ip;
    while (true) {
        char* input_ip = readline("Enter server IP (or 'localhost'): ");
        if (!input_ip) { cleanupNetwork(); return 0; }
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
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) { cleanupNetwork(); return 1; }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(DEFAULT_PORT);
    inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection failed" << std::endl;
        closesocket(sock);
        cleanupNetwork();
        return 1;
    }

    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();
    client_ctx = SSL_CTX_new(TLS_client_method());
    SSL_CTX_set_verify(client_ctx, SSL_VERIFY_NONE, NULL);

    ssl_conn = SSL_new(client_ctx);
    SSL_set_fd(ssl_conn, static_cast<int>(sock));

    if (SSL_connect(ssl_conn) <= 0) {
        std::cerr << "Secure TLS handshake failed." << std::endl;
        SSL_free(ssl_conn);
        closesocket(sock);
        SSL_CTX_free(client_ctx);
        cleanupNetwork();
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
        int bytes = SSL_read(ssl_conn, buffer, BUFFER_SIZE - 1);
        if (bytes <= 0) {
            SSL_free(ssl_conn);
            closesocket(sock);
            SSL_CTX_free(client_ctx);
            cleanupNetwork();
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

    std::thread recv_thread(receiveThread);
    using_history();
    std::string prompt = "[" + my_username + "]: ";

    while (running) {
        char* line = readline(prompt.c_str());
        if (!line) break;
        std::string input(line);
        free(line);
        if (input.empty()) continue;

        add_history(input.c_str());
        if (input == "/quit") {
            running = false; // ИСПРАВЛЕНО: Выставляем сразу, чтобы заблокировать перерисовку readline в потоке приёма
            sendCommand(CMD_QUIT);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
#ifdef _WIN32
            shutdown(sock, SD_BOTH);
#else
            shutdown(sock, SHUT_RDWR);
#endif
            break;
        }

        if (input == "/help") {
            std::cout << "\033[A\r\033[K" << COLOR_YELLOW
                << "--- Available Commands Context Menu ---\n"
                << "/chat [username]   - Open private message session with a user\n"
                << "/group [#name]     - Create or enter a group channel (must start with #)\n"
                << "/online            - Fetch list of all active users in a column\n"
                << "/clear             - Delete entire chat history log files permanently\n"
                << "/exit              - Close current active chat context menu safely\n"
                << "/quit              - Shut down the client session and disconnect\n"
                << "/reply [text]      - Fast quote reply to the last incoming message\n"
                << "/forward           - Forward the last received text to this chat channel\n"
                << "\n--- Special Group Admin Commands ---\n"
                << "/add [username]    - Invite and bind user to this private group\n"
                << "/delete [username] - Kick/remove member from group access list\n"
                << "/delete_group      - Completely drop group stack and destroy its backup file\n"
                << "----------------------------------------"
                << COLOR_RESET << std::endl;
            continue;
        }

        if (input == "/online") {
            sendCommand(CMD_REQ_ONLINE);
            continue;
        }

        stateMutex.lock();
        std::string current_partner = active_chat_partner;
        std::string current_group = active_group;
        std::string l_sender = last_msg_sender;
        std::string l_text = last_msg_text;
        stateMutex.unlock();

        if (input == "/clear") {
            if (!current_partner.empty()) {
                sendCommand(CMD_CLEAR + "|PM|" + current_partner);
            }
            else if (!current_group.empty()) {
                sendCommand(CMD_CLEAR + "|GROUP|" + current_group);
            }
            else {
                std::cout << "\033[A\r\033[K" << COLOR_RED << "[Error]: You must be inside a chat room or group to clear history." << COLOR_RESET << std::endl;
            }
            continue;
        }

        if (!current_partner.empty()) {
            if (input == "/exit") {
                std::cout << "\033[A\r\033[K" << COLOR_YELLOW << "[You left the chat with " << current_partner << "]" << COLOR_RESET << std::endl;
                std::cout << "========================================" << std::endl;
                stateMutex.lock();
                active_chat_partner = "";
                stateMutex.unlock();
                continue;
            }

            if (input.rfind("/reply ", 0) == 0) {
                std::string reply_payload = input.substr(7);
                if (l_text.empty()) {
                    std::cout << "\033[A\r\033[K" << COLOR_RED << "[Error]: No message available to reply to." << COLOR_RESET << std::endl;
                    continue;
                }
                std::string formatted = "(reply to " + l_sender + ": \"" + l_text + "\") " + reply_payload;
                std::cout << "\033[A\r\033[K[" << my_username << "]: " << formatted << std::endl;
                sendCommand(CMD_MSG + "|" + current_partner + "|" + formatted);
                logMessage(CLIENT_LOG, "OUT", "To " + current_partner + ": " + formatted);
                continue;
            }

            if (input == "/forward") {
                if (l_text.empty()) {
                    std::cout << "\033[A\r\033[K" << COLOR_RED << "[Error]: No message available to forward." << COLOR_RESET << std::endl;
                    continue;
                }
                std::string formatted = "(fwd from " + l_sender + "): " + l_text;
                std::cout << "\033[A\r\033[K[" << my_username << "]: " << formatted << std::endl;
                sendCommand(CMD_MSG + "|" + current_partner + "|" + formatted);
                logMessage(CLIENT_LOG, "OUT", "To " + current_partner + ": " + formatted);
                continue;
            }

            std::cout << "\033[A\r\033[K[" << my_username << "]: " << input << std::endl;
            sendCommand(CMD_MSG + "|" + current_partner + "|" + input);
            logMessage(CLIENT_LOG, "OUT", "To " + current_partner + ": " + input);
        }
        else if (!current_group.empty()) {
            if (input == "/exit") {
                std::cout << "\033[A\r\033[K" << COLOR_YELLOW << "[You left the group context " << current_group << "]" << COLOR_RESET << std::endl;
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

            if (input.rfind("/reply ", 0) == 0) {
                std::string reply_payload = input.substr(7);
                if (l_text.empty()) {
                    std::cout << "\033[A\r\033[K" << COLOR_RED << "[Error]: No message available to reply to." << COLOR_RESET << std::endl;
                    continue;
                }
                std::string formatted = "(reply to " + l_sender + ": \"" + l_text + "\") " + reply_payload;
                std::cout << "\033[A\r\033[K[" << my_username << "]: " << formatted << std::endl;
                sendCommand(CMD_GROUP_MSG + "|" + current_group + "|" + formatted);
                logMessage(CLIENT_LOG, "G_OUT", "[" + current_group + "] " + formatted);
                continue;
            }

            if (input == "/forward") {
                if (l_text.empty()) {
                    std::cout << "\033[A\r\033[K" << COLOR_RED << "[Error]: No message available to forward." << COLOR_RESET << std::endl;
                    continue;
                }
                std::string formatted = "(fwd from " + l_sender + "): " + l_text;
                std::cout << "\033[A\r\033[K[" << my_username << "]: " << formatted << std::endl;
                sendCommand(CMD_GROUP_MSG + "|" + current_group + "|" + formatted);
                logMessage(CLIENT_LOG, "G_OUT", "[" + current_group + "] " + formatted);
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
                std::cout << "\033[A\r\033[K\n========================================\n Chat with: " << target << "\n========================================\nType /exit to leave\n" << std::endl;

                sendCommand(CMD_REQ_HISTORY + "|PM|" + target);
            }
            else if (input.rfind("/group ", 0) == 0) {
                std::string gname = input.substr(7);
                if (gname.empty() || gname[0] != '#') {
                    std::cout << "\033[A\r\033[K" << COLOR_RED << "[Error]: Group name must start with '#'" << COLOR_RESET << std::endl;
                    continue;
                }

                std::cout << "\033[A\r\033[K" << std::flush;
                stateMutex.lock();
                pending_group = gname;
                stateMutex.unlock();

                sendCommand(CMD_GROUP_JOIN + "|" + gname);
            }
            else {
                std::cout << "\033[A\r\033[K" << COLOR_YELLOW << "Use: /chat <name>, /group <#name>, /online or /help" << COLOR_RESET << std::endl;
            }
        }
    }

    rl_cleanup_after_signal();
    if (ssl_conn) {
        SSL_shutdown(ssl_conn);
        SSL_free(ssl_conn);
    }
    closesocket(sock);
    if (client_ctx) SSL_CTX_free(client_ctx);
    if (recv_thread.joinable()) recv_thread.join();
    cleanupNetwork();
    return 0;
}
