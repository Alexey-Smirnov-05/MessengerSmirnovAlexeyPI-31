// client.cpp
// Команда для сборки в MINGW64 / UCRT64:
// g++ -D_XOPEN_SOURCE_EXTENDED=1 -DNCURSES_WIDECHAR=1 client.cpp logger.cpp -I/ucrt64/include/ncursesw -lncursesw -lssl -lcrypto -lws2_32 -o client.exe
// Запуск на Windows: winpty ./client.exe
// Сборка на Linux:  g++ -D_XOPEN_SOURCE_EXTENDED=1 client.cpp logger.cpp -lncursesw -lssl -lcrypto -pthread -o client

#define _XOPEN_SOURCE_EXTENDED 1
#define NCURSES_WIDECHAR 1

#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <thread>
#include <vector>
#include <cwchar>
#include <clocale>
#include <chrono>

#include <ncursesw/ncurses.h>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
<<<<<<< HEAD
=======
#  include <direct.h>  
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
#else
#  include <arpa/inet.h>
#  include <sys/socket.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <sys/select.h>
<<<<<<< HEAD
=======
#  include <sys/stat.h> 
#  include <sys/types.h>
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include "common.h"
#include "logger.h"

<<<<<<< HEAD
=======
// Гарантируем компиляцию новых макросов
#ifndef CMD_REQ_GROUP_USERS
#  define CMD_REQ_GROUP_USERS "REQ_GROUP_USERS"
#endif
#ifndef CMD_LIST_GROUP_USERS
#  define CMD_LIST_GROUP_USERS "LIST_GROUP_USERS"
#endif

>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
// =============================================================================
// ГЛОБАЛЬНЫЕ СТРУКТУРЫ И ДАННЫЕ
// =============================================================================

<<<<<<< HEAD
// Структура для хранения одного сообщения в истории чата
struct DisplayMessage {
    std::wstring sender;     // Имя отправителя
    std::wstring text;       // Текст сообщения
    int color_pair;          // Цветовая схема сообщения
    bool is_system;          // Флаг системного уведомления
=======
struct DisplayMessage {
    std::wstring sender;
    std::wstring text;
    int color_pair;
    bool is_system;
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
};

std::string CLIENT_LOG;
SOCKET sock = INVALID_SOCKET;
SSL* ssl_conn = nullptr;
SSL_CTX* client_ctx = nullptr;

<<<<<<< HEAD
// Флаг работы клиента (atomic для безопасного доступа из разных потоков)
=======
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
std::atomic<bool> running(true);

std::wstring my_username = L"";
std::wstring active_chat_partner = L"";
std::wstring active_group = L"";
std::wstring pending_group = L"";

<<<<<<< HEAD
// Вектор для хранения истории сообщений текущего экрана
std::vector<DisplayMessage> chat_history;

bool has_forward_buffer = false;
DisplayMessage forward_buffer;

// Режимы работы пользовательского интерфейса
enum UiMode { MODE_INPUT, MODE_SELECT };
UiMode current_mode = MODE_INPUT;

int selected_line_idx = -1;  // Индекс выбранной строки в режиме MODE_SELECT
int scroll_offset = 0;       // Смещение прокрутки чата

std::mutex stateMutex;       // Мьютекс для защиты состояния (переменных чата)
std::mutex ncursesMtx;       // Мьютекс для защиты вызовов библиотеки ncurses

// Указатели на окна интерфейса
=======
std::vector<DisplayMessage> chat_history;

bool has_forward_buffer = false;
DisplayMessage forward_buffer;

enum UiMode { MODE_INPUT, MODE_SELECT };
UiMode current_mode = MODE_INPUT;

int selected_line_idx = -1;
int scroll_offset = 0;

std::mutex stateMutex;
std::mutex ncursesMtx;

>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
WINDOW* header_win = nullptr;
WINDOW* chat_win = nullptr;
WINDOW* input_win = nullptr;

std::wstring current_input_string = L"";

// =============================================================================
<<<<<<< HEAD
// UTF-8 КОНВЕРТЕРЫ (Без устаревшего std::wstring_convert)
=======
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ИНИЦИАЛИЗАЦИИ И ДИРЕКТОРИЙ
// =============================================================================

void create_directory(const std::string& folder_name) {
#ifdef _WIN32
    _mkdir(folder_name.c_str());
#else
    mkdir(folder_name.c_str(), 0777);
#endif
}

void init_project_structure() {
    create_directory("logs");
    create_directory("chats");
    create_directory("groups");
}

// =============================================================================
// UTF-8 КОНВЕРТЕРЫ 
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
// =============================================================================

std::wstring to_wstring(const std::string& utf8) {
    std::wstring r;
    const unsigned char* s = reinterpret_cast<const unsigned char*>(utf8.c_str());
    size_t i = 0, n = utf8.size();
    while (i < n) {
        unsigned char c = s[i];
        wchar_t cp = 0;
        if (c < 0x80) { cp = c; i += 1; }
        else if ((c & 0xE0) == 0xC0 && i + 1 < n) { cp = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F); i += 2; }
        else if ((c & 0xF0) == 0xE0 && i + 2 < n) { cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F); i += 3; }
        else if ((c & 0xF8) == 0xF0 && i + 3 < n) { cp = ((c & 0x07) << 18) | ((s[i + 1] & 0x3F) << 12) | ((s[i + 2] & 0x3F) << 6) | (s[i + 3] & 0x3F); i += 4; }
        else { i += 1; continue; }
        r.push_back(cp);
    }
    return r;
}

std::string to_string(const std::wstring& ws) {
    std::string r;
    for (wchar_t wc : ws) {
        unsigned u = (unsigned)wc;
        if (u < 0x80) { r += (char)u; }
        else if (u < 0x800) { r += (char)(0xC0 | (u >> 6));   r += (char)(0x80 | (u & 0x3F)); }
        else if (u < 0x10000) { r += (char)(0xE0 | (u >> 12));  r += (char)(0x80 | ((u >> 6) & 0x3F));  r += (char)(0x80 | (u & 0x3F)); }
        else { r += (char)(0xF0 | (u >> 18));  r += (char)(0x80 | ((u >> 12) & 0x3F)); r += (char)(0x80 | ((u >> 6) & 0x3F)); r += (char)(0x80 | (u & 0x3F)); }
    }
    return r;
}

// =============================================================================
<<<<<<< HEAD
// ЧТЕНИЕ КЛАВИАТУРЫ (КРОССПЛАТФОРМЕННЫЙ ФИКС КИРИЛЛИЦЫ)
=======
// ЧТЕНИЕ КЛАВИАТУРЫ 
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
// =============================================================================

static int read_key() {
    wint_t ch = 0;
    int res = wget_wch(stdscr, &ch);

    if (res == ERR) return 0;

    bool is_known_special = (
        ch == (wint_t)KEY_UP || ch == (wint_t)KEY_DOWN ||
        ch == (wint_t)KEY_LEFT || ch == (wint_t)KEY_RIGHT ||
        ch == (wint_t)KEY_BACKSPACE || ch == (wint_t)KEY_DC ||
        ch == (wint_t)KEY_HOME || ch == (wint_t)KEY_END ||
        ch == (wint_t)KEY_PPAGE || ch == (wint_t)KEY_NPAGE ||
        ch == (wint_t)KEY_ENTER || ch == (wint_t)KEY_RESIZE ||
        (ch >= (wint_t)KEY_F(1) && ch <= (wint_t)KEY_F(12))
        );

    if (res == KEY_CODE_YES) {
        if (!is_known_special && ch >= 32) {
<<<<<<< HEAD
            return (int)ch;  // Возвращаем Юникод код кириллицы
=======
            return (int)ch;
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
        }
        if (ch == (wint_t)KEY_BACKSPACE || ch == (wint_t)KEY_DC) return -KEY_BACKSPACE;
        if (ch == (wint_t)KEY_UP)     return -KEY_UP;
        if (ch == (wint_t)KEY_DOWN)   return -KEY_DOWN;
        if (ch == (wint_t)KEY_LEFT)   return -KEY_LEFT;
        if (ch == (wint_t)KEY_RIGHT)  return -KEY_RIGHT;
        if (ch == (wint_t)KEY_F(2))   return -KEY_F(2);
        if (ch == (wint_t)KEY_RESIZE) return -KEY_RESIZE;
        if (ch == (wint_t)KEY_ENTER)  return '\n';
        return 0;
    }

<<<<<<< HEAD
    if (ch == 27) return -27;        // ESC
=======
    if (ch == 27) return -27;
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    if (ch == '\n' || ch == '\r') return '\n';
    if (ch == 127 || ch == 8) return -KEY_BACKSPACE;
    if (ch >= 32) return (int)ch;
    return 0;
}

// =============================================================================
<<<<<<< HEAD
// ТАЙМАУТ ПОДКЛЮЧЕНИЯ (Неблокирующий connect)
=======
// ТАЙМАУТ ПОДКЛЮЧЕНИЯ 
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
// =============================================================================

bool connectWithTimeout(SOCKET s, const sockaddr* addr, int addrlen, int timeout_sec) {
#ifdef _WIN32
    unsigned long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif

    connect(s, addr, addrlen);

    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(s, &writeSet);
    timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;

    int res = select(static_cast<int>(s + 1), nullptr, &writeSet, nullptr, &tv);

#ifdef _WIN32
    mode = 0;
    ioctlsocket(s, FIONBIO, &mode);
#else
    fcntl(s, F_SETFL, flags);
#endif

    return (res > 0 && FD_ISSET(s, &writeSet));
}

// =============================================================================
// UI ОТРИСОВКА И ОБНОВЛЕНИЕ ЭКРАНА
// =============================================================================

int getWStringDisplayWidth(const std::wstring& ws) {
    if (ws.empty()) return 0;
#ifndef _WIN32
    int w = wcswidth(ws.c_str(), ws.size());
    if (w >= 0) return w;
#endif
    return (int)ws.size();
}

void updateHeader() {
    if (!header_win) return;
    werase(header_win);

<<<<<<< HEAD
    // Белая шапка с черным жирным текстом для высокой читаемости на Windows
    wattron(header_win, COLOR_PAIR(5) | A_BOLD);
=======
    // Белая шапка с черным текстом
    wbkgd(header_win, COLOR_PAIR(5));
    wattron(header_win, COLOR_PAIR(5));
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    int my, mx; getmaxyx(header_win, my, mx); (void)my;

    std::wstring s = L" Secure Messenger | User: " + my_username;
    if (!active_chat_partner.empty()) s += L" | Chat: " + active_chat_partner;
    else if (!active_group.empty())   s += L" | Group: " + active_group;
    else                              s += L" | Main Menu";

    if (current_mode == MODE_SELECT)  s += L" | [SELECT MODE: Arrows, R-Reply, F-Forward, ESC-Exit]";
    else                              s += L" | [F2-Select Mode]";

    if (has_forward_buffer)           s += L" | (FWD Ready)";

    int dw = getWStringDisplayWidth(s);
    if (dw < mx) s.append(mx - dw, L' '); else s = s.substr(0, mx);

    mvwaddwstr(header_win, 0, 0, s.c_str());
<<<<<<< HEAD
    wattroff(header_win, COLOR_PAIR(5) | A_BOLD);
=======
    wattroff(header_win, COLOR_PAIR(5));
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    wnoutrefresh(header_win);
}

void updateInputWin() {
    if (!input_win) return;
    werase(input_win);
    if (current_mode == MODE_SELECT) {
        mvwaddwstr(input_win, 0, 0, L"[SELECT MODE] Use Up/Down arrows. Press ESC to cancel.");
    }
    else {
        std::wstring p = L"[" + my_username + L"]: ";
        mvwaddwstr(input_win, 0, 0, p.c_str());
        waddwstr(input_win, current_input_string.c_str());
        wmove(input_win, 0, getWStringDisplayWidth(p) + getWStringDisplayWidth(current_input_string));
    }
    wnoutrefresh(input_win);
}

void redrawChatWindow() {
    if (!chat_win) return;
    werase(chat_win);
    int my, mx; getmaxyx(chat_win, my, mx);
    if (chat_history.empty()) { wnoutrefresh(chat_win); return; }
    int total = (int)chat_history.size();
    if (current_mode == MODE_INPUT)
        scroll_offset = (total > my) ? total - my : 0;
    scroll_offset = std::max(0, std::min(scroll_offset, (total > my ? total - my : 0)));
    int row = 0;
    for (int i = scroll_offset; i < std::min(scroll_offset + my, total); ++i, ++row) {
        bool sel = (current_mode == MODE_SELECT && i == selected_line_idx);
        wmove(chat_win, row, 0); wclrtoeol(chat_win);
        if (sel) wattron(chat_win, A_STANDOUT);

<<<<<<< HEAD
        // ЦВЕТОВАЯ ДИФФЕРЕНЦИАЦИЯ: Свои сообщения — Ярко-желтые, Собеседника — Белые
        int color = chat_history[i].color_pair;
        if (!chat_history[i].is_system) {
            if (chat_history[i].sender == my_username) {
                color = 1; // Желтый цвет для моих сообщений
            }
            else {
                color = 6; // Белый цвет для сообщений других пользователей
=======
        int color = chat_history[i].color_pair;
        if (!chat_history[i].is_system) {
            if (chat_history[i].sender == my_username) {
                color = 1;  // Желтый цвет для моих сообщений
            }
            else {
                color = 6;  // Белый цвет для сообщений других
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
            }
        }

        if (color > 0) wattron(chat_win, COLOR_PAIR(color));
        std::wstring line = chat_history[i].is_system
            ? chat_history[i].text
            : L"[" + chat_history[i].sender + L"]: " + chat_history[i].text;
        if (getWStringDisplayWidth(line) > mx) line = line.substr(0, mx - 3) + L"...";
        mvwaddwstr(chat_win, row, 0, line.c_str());
        if (color > 0) wattroff(chat_win, COLOR_PAIR(color));
        if (sel) wattroff(chat_win, A_STANDOUT);
    }
    wnoutrefresh(chat_win);
}

void triggerGlobalUpdate() {
    std::lock_guard<std::mutex> lk(ncursesMtx);
    updateHeader(); redrawChatWindow(); updateInputWin();
    curs_set(current_mode == MODE_INPUT ? 1 : 0);
    doupdate();
}

void pushMessageToHistory(const std::wstring& sender, const std::wstring& text,
    bool is_system = false, int color_pair = 0) {
    std::lock_guard<std::mutex> lk(stateMutex);
    chat_history.push_back({ sender, text, color_pair, is_system });
    if (current_mode == MODE_INPUT)
        selected_line_idx = (int)chat_history.size() - 1;
}

void resizeUI() {
    std::lock_guard<std::mutex> lk(stateMutex);
    int my, mx; getmaxyx(stdscr, my, mx);
    if (my < 5 || mx < 20) return;

<<<<<<< HEAD
    // Очищаем главный экран для предотвращения артефактов ("бегающего текста") при масштабировании
=======
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    clear();
    refresh();

    wresize(header_win, 1, mx);
    wresize(chat_win, my - 3, mx);
    wresize(input_win, 2, mx);
    mvwin(input_win, my - 2, 0);
    stateMutex.unlock();
    triggerGlobalUpdate();
    stateMutex.lock();
}

// =============================================================================
// СЕТЕВАЯ ЛОГИКА И СЕТЬ
// =============================================================================

bool sendCommand(const std::string& cmd) {
    if (!ssl_conn) return false;
    std::string m = cmd + "\n";
    return SSL_write(ssl_conn, m.c_str(), (int)m.size()) > 0;
}

void processIncomingPacket(const std::string& data) {
    std::istringstream iss(data);
    std::string cmd; std::getline(iss, cmd, '|');

    if (cmd == CMD_INMSG) {
        std::string from, msg; std::getline(iss, from, '|'); std::getline(iss, msg);

<<<<<<< HEAD
        // ФИКС КРОСС-СИНХРОНИЗАЦИИ: Используем find() вместо == на случай присутствия невидимого \r
=======
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
        if (msg.find("_SYSTEM_CHAT_CLEAR_REQUEST_") != std::string::npos) {
            std::lock_guard<std::mutex> lk(stateMutex);
            if (to_wstring(from) == active_chat_partner) {
                chat_history.clear();
                chat_history.push_back({ L"System", L"[Chat history cleared by partner]", 2, true });
                selected_line_idx = -1;
                scroll_offset = 0;
            }
        }
        else {
            stateMutex.lock(); std::wstring cp = active_chat_partner; stateMutex.unlock();
            if (to_wstring(from) == cp)
                pushMessageToHistory(to_wstring(from), to_wstring(msg));
            else
                pushMessageToHistory(L"System", L"[Message from " + to_wstring(from) + L"]: " + to_wstring(msg), true, 2);
            logMessage(CLIENT_LOG, "IN", "From " + from + ": " + msg);
        }
    }
    else if (cmd == CMD_ONLINE_LIST) {
        std::string list; std::getline(iss, list);
        std::istringstream oi(list); std::string u;
        pushMessageToHistory(L"System", L"--- Online Users ---", true, 4);
        while (std::getline(oi, u, ',')) if (!u.empty()) pushMessageToHistory(L"System", to_wstring(u), true, 6);
        pushMessageToHistory(L"System", L"--------------------", true, 4);
    }
    else if (cmd == CMD_GROUP_MSG) {
        std::string gn, from, msg;
        std::getline(iss, gn, '|'); std::getline(iss, from, '|'); std::getline(iss, msg);

<<<<<<< HEAD
        // ФИКС КРОСС-СИНХРОНИЗАЦИИ В ГРУППЕ: Используем find() вместо == на случай присутствия невидимого \r
=======
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
        if (msg.find("_SYSTEM_CHAT_CLEAR_REQUEST_") != std::string::npos) {
            std::lock_guard<std::mutex> lk(stateMutex);
            if (to_wstring(gn) == active_group) {
                chat_history.clear();
                chat_history.push_back({ L"System", L"[Chat history cleared by partner]", 2, true });
                selected_line_idx = -1;
                scroll_offset = 0;
            }
        }
        else {
            stateMutex.lock(); std::wstring cg = active_group; stateMutex.unlock();
            if (to_wstring(gn) == cg)
                pushMessageToHistory(to_wstring(from), to_wstring(msg));
            else
                pushMessageToHistory(L"System", L"[Group " + to_wstring(gn) + L" / " + to_wstring(from) + L"]: " + to_wstring(msg), true, 2);
            logMessage(CLIENT_LOG, "G_IN", "[" + gn + "] " + from + ": " + msg);
        }
    }
    else if (cmd == CMD_HIST_LINE) {
        std::string type; std::getline(iss, type, '|');
        if (type == "PM") {
            std::string from, msg; std::getline(iss, from, '|'); std::getline(iss, msg);
            pushMessageToHistory(to_wstring(from), to_wstring(msg));
        }
        else if (type == "GROUP") {
            std::string gn, from, msg;
            std::getline(iss, gn, '|'); std::getline(iss, from, '|'); std::getline(iss, msg);
            pushMessageToHistory(to_wstring(from), to_wstring(msg));
        }
    }
<<<<<<< HEAD
    else if (cmd == CMD_GROUP_NOTIFY) {
        std::string type, gn; std::getline(iss, type, '|'); std::getline(iss, gn, '|');
        stateMutex.lock();
        if (type == "KICKED" && active_group == to_wstring(gn))
        {
            active_group = L""; pushMessageToHistory(L"System", L"[You were kicked from group " + to_wstring(gn) + L"]", true, 3);
        }
        else if (type == "DELETED" && active_group == to_wstring(gn))
        {
            active_group = L""; pushMessageToHistory(L"System", L"[Group " + to_wstring(gn) + L" was deleted by admin]", true, 3);
        }
        stateMutex.unlock();
=======
    else if (cmd == CMD_GROUP_NOTIFY || cmd == "GROUP_NOTIFY") {
        std::string type, gn, admin_user, target_user;
        std::getline(iss, type, '|');
        std::getline(iss, gn, '|');
        std::getline(iss, admin_user, '|');
        std::getline(iss, target_user);

        if (!admin_user.empty() && admin_user.back() == '\r') admin_user.pop_back();
        if (!target_user.empty() && target_user.back() == '\r') target_user.pop_back();

        stateMutex.lock();
        std::wstring w_gn = to_wstring(gn);
        std::wstring w_target = to_wstring(target_user);
        std::wstring w_admin = to_wstring(admin_user);
        stateMutex.unlock();

        if (type == "ADDED") {
            pushMessageToHistory(L"System", L"[User " + w_target + L" was added to group " + w_gn + L" by admin " + w_admin + L"]", true, 2);
            logMessage(CLIENT_LOG, "INFO", "User " + target_user + " was added to group " + gn);
        }
        else if (type == "KICKED" || type == "REMOVED" || type == "KICKED_USER") {
            if (w_target == my_username) {
                stateMutex.lock();
                if (active_group == w_gn) {
                    active_group = L"";
                    chat_history.clear();
                    selected_line_idx = -1;
                    scroll_offset = 0;
                }
                stateMutex.unlock();
                pushMessageToHistory(L"System", L"[You were kicked from group " + w_gn + L" by admin " + w_admin + L"]", true, 3);
                logMessage(CLIENT_LOG, "INFO", "You were kicked from group " + gn);
            }
            else {
                pushMessageToHistory(L"System", L"[User " + w_target + L" was kicked from group " + w_gn + L" by admin " + w_admin + L"]", true, 3);
                logMessage(CLIENT_LOG, "INFO", "User " + target_user + " was kicked from group " + gn);
            }
        }
        else if (type == "DELETED") {
            stateMutex.lock();
            if (active_group == w_gn) {
                active_group = L"";
                chat_history.clear();
                selected_line_idx = -1;
                scroll_offset = 0;
            }
            stateMutex.unlock();
            pushMessageToHistory(L"System", L"[Group " + w_gn + L" was deleted by admin]", true, 3);
            logMessage(CLIENT_LOG, "INFO", "Group " + gn + " was deleted");
        }
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    }
    else if (cmd == CMD_OK) {
        std::string info; std::getline(iss, info);
        stateMutex.lock();
        if (!pending_group.empty() &&
            (info.find("Group created") != std::string::npos || info.find("Joined group") != std::string::npos)) {
            active_group = pending_group; std::wstring gn = active_group; pending_group = L"";
<<<<<<< HEAD
            stateMutex.unlock();
            chat_history.clear(); selected_line_idx = -1; scroll_offset = 0;
            // Убрана дублирующая системная строка "=== Группа ===", так как имя группы уже выводится в верхней шапке (Header)
            sendCommand(CMD_REQ_HISTORY + "|GROUP|" + to_string(gn));
=======

            chat_history.clear(); selected_line_idx = -1; scroll_offset = 0;
            logMessage(CLIENT_LOG, "INFO", "Joined group " + to_string(gn));
            stateMutex.unlock();
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
        }
        else { stateMutex.unlock(); }
    }
    else if (cmd == CMD_ERROR) {
        std::string err; std::getline(iss, err);
        pushMessageToHistory(L"Error", to_wstring(err), true, 3);
        std::lock_guard<std::mutex> lk(stateMutex); pending_group = L"";
    }
<<<<<<< HEAD
    // СИНХРОННАЯ ОЧИСТКА ЭКРАНА: Если от сервера напрямую прилетела команда CLEAR
=======
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    else if (cmd == CMD_CLEAR) {
        std::string clear_type, sender_or_group;
        std::getline(iss, clear_type, '|');
        std::getline(iss, sender_or_group);
        std::wstring target = to_wstring(sender_or_group);

        std::lock_guard<std::mutex> lk(stateMutex);
        bool needs_local_clear = false;

<<<<<<< HEAD
        // Если это личный чат, и имя собеседника совпадает с тем, кто очистил, либо с моим именем (для надежной кросс-маршрутизации сервера)
        if (clear_type == "PM" && (target == active_chat_partner || target == my_username)) {
            needs_local_clear = true;
        }
        // Если это групповой чат, и мы находимся в этой группе
=======
        if (clear_type == "PM" && (target == active_chat_partner || target == my_username)) {
            needs_local_clear = true;
        }
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
        else if (clear_type == "GROUP" && target == active_group) {
            needs_local_clear = true;
        }

        if (needs_local_clear) {
            chat_history.clear();
            chat_history.push_back({ L"System", L"[Chat history cleared by partner]", 2, true });
            selected_line_idx = -1;
            scroll_offset = 0;
<<<<<<< HEAD
        }
    }
=======
            logMessage(CLIENT_LOG, "INFO", "Chat history was cleared remotely.");
        }
    }
    else if (cmd == "GROUP_USERS" || cmd == "LIST_GROUP_USERS" || cmd == "GROUP_MEMBERS" || cmd == std::string(CMD_LIST_GROUP_USERS)) {
        std::string list; std::getline(iss, list);
        if (!list.empty() && list.back() == '\r') list.pop_back();

        std::istringstream list_stream(list);
        std::string group_user;

        pushMessageToHistory(L"System", L"--- Group Members ---", true, 4);
        while (std::getline(list_stream, group_user, ',')) {
            if (!group_user.empty()) {
                pushMessageToHistory(L"System", to_wstring(group_user), true, 6);
            }
        }
        pushMessageToHistory(L"System", L"---------------------", true, 4);
    }
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    triggerGlobalUpdate();
}

void receiveThread() {
    char buf[BUFFER_SIZE]; std::string sbuf;
    while (running) {
        memset(buf, 0, BUFFER_SIZE);
        int n = SSL_read(ssl_conn, buf, BUFFER_SIZE - 1);
        if (n <= 0) {
            if (running) { pushMessageToHistory(L"System", L"[Connection to server lost]", true, 3); triggerGlobalUpdate(); running = false; }
            break;
        }
        sbuf += std::string(buf, n);
        size_t pos;
        while ((pos = sbuf.find('\n')) != std::string::npos) {
            std::string pkt = sbuf.substr(0, pos); sbuf.erase(0, pos + 1);
<<<<<<< HEAD

            // ФИКС КАРЕТКИ: Обязательно отсекаем символ \r, иначе он сломает сравнения строк и парсинг команд
            if (!pkt.empty() && pkt.back() == '\r') {
                pkt.pop_back();
            }

=======
            if (!pkt.empty() && pkt.back() == '\r') {
                pkt.pop_back();
            }
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
            if (!pkt.empty()) processIncomingPacket(pkt);
        }
    }
}

// =============================================================================
// ОБРАБОТКА КОМАНД ПОЛЬЗОВАТЕЛЯ
// =============================================================================

void handleOutboundCommand(const std::wstring& wi) {
    std::string input = to_string(wi);
<<<<<<< HEAD
    if (input == "/quit") { running = false; sendCommand(CMD_QUIT); return; }
    if (input == "/online") { sendCommand(CMD_REQ_ONLINE); return; }

    // СТРОГОЕ СООТВЕТСТВИЕ КОМАНД В СПРАВКЕ РЕАЛЬНОМУ ФУНКЦИОНАЛУ
=======
    if (input == "/quit") {
        logMessage(CLIENT_LOG, "INFO", "User quit application.");
        running = false; sendCommand(CMD_QUIT); return;
    }
    if (input == "/online") { sendCommand(CMD_REQ_ONLINE); return; }

>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    if (input == "/help") {
        pushMessageToHistory(L"System", L"--- Active Commands ---", true, 4);
        pushMessageToHistory(L"System", L"/chat [user]   - Open a private chat with a user", true, 4);
        pushMessageToHistory(L"System", L"/group [#name] - Join or create a group channel", true, 4);
        pushMessageToHistory(L"System", L"/add [user]    - Add a user to the current group", true, 4);
        pushMessageToHistory(L"System", L"/delete [user] - Kick a user from the current group", true, 4);
        pushMessageToHistory(L"System", L"/delete_group  - Delete the current group", true, 4);
<<<<<<< HEAD
=======
        pushMessageToHistory(L"System", L"/users         - List current group members", true, 4);
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
        pushMessageToHistory(L"System", L"/online        - Get the list of online users", true, 4);
        pushMessageToHistory(L"System", L"/clear         - Clear the chat screen locally", true, 4);
        pushMessageToHistory(L"System", L"/exit          - Leave the current private chat or group", true, 4);
        pushMessageToHistory(L"System", L"/quit          - Exit the messenger application", true, 4);
        pushMessageToHistory(L"System", L"Press F2 to enter Select Mode (Scroll, Reply 'R', Forward 'F')", true, 2);
        triggerGlobalUpdate(); return;
    }
    stateMutex.lock(); std::wstring cp = active_chat_partner, cg = active_group; stateMutex.unlock();

<<<<<<< HEAD
    // ИСПРАВЛЕННЫЙ /CLEAR БЕЗ DEADLOCK (Блокировка stateMutex теперь освобождается корректно)
=======
    if (input == "/users") {
        if (cg.empty()) {
            pushMessageToHistory(L"System", L"Error: You must be inside a group to list its members!", true, 3);
            triggerGlobalUpdate();
            return;
        }
        sendCommand(std::string(CMD_REQ_GROUP_USERS) + "|" + to_string(cg));
        logMessage(CLIENT_LOG, "INFO", "Requested users list for group " + to_string(cg));
        return;
    }

>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    if (input == "/clear") {
        {
            std::lock_guard<std::mutex> lk(stateMutex);
            chat_history.clear();
            chat_history.push_back({ L"System", L"[Chat history cleared locally]", 2, true });
            selected_line_idx = -1;
            scroll_offset = 0;
        }
        triggerGlobalUpdate();

        if (!cp.empty()) {
            sendCommand(CMD_CLEAR + "|PM|" + to_string(cp));
<<<<<<< HEAD
            // Отсылаем партнеру в реальном времени скрытое управляющее сообщение для мгновенного очищения экрана
            sendCommand(CMD_MSG + "|" + to_string(cp) + "|_SYSTEM_CHAT_CLEAR_REQUEST_");
        }
        else if (!cg.empty()) {
            sendCommand(CMD_CLEAR + "|GROUP|" + to_string(cg));
            // Отсылаем во всю группу скрытое управляющее сообщение для мгновенного очищения экрана
            sendCommand(CMD_GROUP_MSG + "|" + to_string(cg) + "|_SYSTEM_CHAT_CLEAR_REQUEST_");
=======
            sendCommand(CMD_MSG + "|" + to_string(cp) + "|_SYSTEM_CHAT_CLEAR_REQUEST_");
            logMessage(CLIENT_LOG, "INFO", "Cleared PM history with " + to_string(cp));
        }
        else if (!cg.empty()) {
            sendCommand(CMD_CLEAR + "|GROUP|" + to_string(cg));
            sendCommand(CMD_GROUP_MSG + "|" + to_string(cg) + "|_SYSTEM_CHAT_CLEAR_REQUEST_");
            logMessage(CLIENT_LOG, "INFO", "Cleared GROUP history for " + to_string(cg));
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
        }
        return;
    }
    if (!cp.empty()) {
        if (input == "/exit") {
            pushMessageToHistory(L"System", L"[Left chat with " + cp + L"]", true, 4);
<<<<<<< HEAD
=======
            logMessage(CLIENT_LOG, "INFO", "Left private chat with " + to_string(cp));
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
            std::lock_guard<std::mutex> lk(stateMutex);
            active_chat_partner = L""; chat_history.clear(); selected_line_idx = -1; scroll_offset = 0;
            return;
        }
<<<<<<< HEAD
        pushMessageToHistory(my_username, wi); // Мои отправленные сообщения будут желтыми
        sendCommand(CMD_MSG + "|" + to_string(cp) + "|" + input);
=======
        pushMessageToHistory(my_username, wi);
        sendCommand(CMD_MSG + "|" + to_string(cp) + "|" + input);
        logMessage(CLIENT_LOG, "OUT", "To " + to_string(cp) + ": " + input); // ПОЛНОЦЕННОЕ ЛОГИРОВАНИЕ ИСХОДЯЩИХ
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    }
    else if (!cg.empty()) {
        if (input == "/exit") {
            pushMessageToHistory(L"System", L"[Left group]", true, 4);
<<<<<<< HEAD
=======
            logMessage(CLIENT_LOG, "INFO", "Left group " + to_string(cg));
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
            std::lock_guard<std::mutex> lk(stateMutex);
            active_group = L""; chat_history.clear(); selected_line_idx = -1; scroll_offset = 0;
            return;
        }
<<<<<<< HEAD
        if (input.rfind("/add ", 0) == 0) { sendCommand(CMD_GROUP_ADD + "|" + to_string(cg) + "|" + input.substr(5)); return; }
        if (input.rfind("/delete ", 0) == 0) { sendCommand(CMD_GROUP_KICK + "|" + to_string(cg) + "|" + input.substr(8)); return; }
        if (input == "/delete_group") { sendCommand(CMD_GROUP_DEL + "|" + to_string(cg)); return; }
        pushMessageToHistory(my_username, wi);
        sendCommand(CMD_GROUP_MSG + "|" + to_string(cg) + "|" + input);
=======
        if (input.rfind("/add ", 0) == 0) {
            sendCommand(CMD_GROUP_ADD + "|" + to_string(cg) + "|" + input.substr(5));
            logMessage(CLIENT_LOG, "INFO", "Sent request to add user " + input.substr(5) + " to group " + to_string(cg));
            return;
        }
        if (input.rfind("/delete ", 0) == 0) {
            sendCommand(CMD_GROUP_KICK + "|" + to_string(cg) + "|" + input.substr(8));
            logMessage(CLIENT_LOG, "INFO", "Sent request to kick user " + input.substr(8) + " from group " + to_string(cg));
            return;
        }
        if (input == "/delete_group") {
            sendCommand(CMD_GROUP_DEL + "|" + to_string(cg));
            logMessage(CLIENT_LOG, "INFO", "Sent request to delete group " + to_string(cg));
            return;
        }
        pushMessageToHistory(my_username, wi);
        sendCommand(CMD_GROUP_MSG + "|" + to_string(cg) + "|" + input);
        logMessage(CLIENT_LOG, "G_OUT", "[" + to_string(cg) + "] " + input); // ПОЛНОЦЕННОЕ ЛОГИРОВАНИЕ ГРУППОВЫХ ИСХОДЯЩИХ
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    }
    else {
        if (input.rfind("/chat ", 0) == 0) {
            std::string t = input.substr(6);
            if (to_wstring(t) == my_username) return;
            stateMutex.lock(); active_chat_partner = to_wstring(t); chat_history.clear(); selected_line_idx = -1; scroll_offset = 0; stateMutex.unlock();

<<<<<<< HEAD
            // Убрана бессмысленная системная строка "=== Чат с: ... ===" (название уже отображается на верхней панели)
            sendCommand(CMD_REQ_HISTORY + "|PM|" + t);
=======
            sendCommand(CMD_REQ_HISTORY + "|PM|" + t);
            logMessage(CLIENT_LOG, "INFO", "Opened private chat with " + t);
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
        }
        else if (input.rfind("/group ", 0) == 0) {
            std::string gn = input.substr(7);
            if (gn.empty() || gn[0] != '#') { pushMessageToHistory(L"System", L"Group name must start with '#'", true, 3); return; }
            std::lock_guard<std::mutex> lk(stateMutex); pending_group = to_wstring(gn);
            sendCommand(CMD_GROUP_JOIN + "|" + gn);
        }
        else {
            pushMessageToHistory(L"System", L"Type /chat <name>, /group <#name>, /online or /help", true, 4);
        }
    }
}

// =============================================================================
// ВВОД НА ЭКРАНАХ ЛОГИНА/IP
// =============================================================================

std::wstring readSimpleInput(const std::wstring& prompt, const std::wstring& err_msg = L"") {
    std::wstring input;
    while (true) {
        werase(stdscr);
        if (!err_msg.empty()) { attron(COLOR_PAIR(3)); mvaddwstr(1, 2, err_msg.c_str()); attroff(COLOR_PAIR(3)); }
        mvaddwstr(3, 2, prompt.c_str());
        mvaddwstr(4, 2, input.c_str());
        curs_set(1);
        move(4, 2 + getWStringDisplayWidth(input));
        refresh();
        int k = read_key();
        if (k == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
        if (k == '\n') return input;
        if (k == -KEY_BACKSPACE) { if (!input.empty()) input.pop_back(); continue; }
        if (k == -27) continue;
        if (k > 0 && k >= 32) input.push_back((wchar_t)k);
    }
}

// =============================================================================
// MAIN (ТОЧКА ВХОДА)
// =============================================================================

int main() {
<<<<<<< HEAD
    // --- Локаль ДО initscr ---
#ifdef _WIN32
    // Полная очистка консоли Windows для запрета скролла вверх до предыдущих команд консоли
=======
    init_project_structure();

#ifdef _WIN32
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    system("cls");
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    _putenv("LANG=ru_RU.UTF-8");
    const char* locs[] = { "ru_RU.UTF-8", "en_US.UTF-8", "C.UTF-8", ".UTF-8", "", nullptr };
    for (int i = 0; locs[i]; ++i) if (setlocale(LC_ALL, locs[i])) break;
#else
    setlocale(LC_ALL, "");
#endif

<<<<<<< HEAD
    // --- Инициализация ncurses ---
=======
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    initscr();
    set_escdelay(25);
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    meta(stdscr, TRUE);
    start_color();

<<<<<<< HEAD
    // Настройка цветовой палитры
    init_pair(1, COLOR_YELLOW, COLOR_BLACK); // Мои отправленные сообщения (Желтые)
    init_pair(2, COLOR_GREEN, COLOR_BLACK); // Системный успех/уведомления
    init_pair(3, COLOR_RED, COLOR_BLACK); // Системные ошибки
    init_pair(4, COLOR_CYAN, COLOR_BLACK); // Информационные логи/справка
    init_pair(5, COLOR_BLACK, COLOR_WHITE); // Черный текст на белом фоне для Header Windows
    init_pair(6, COLOR_WHITE, COLOR_BLACK); // Сообщения собеседника (Белые)

    // --- Экран ввода IP ---
=======
    init_pair(1, COLOR_YELLOW, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_RED, COLOR_BLACK);
    init_pair(4, COLOR_CYAN, COLOR_BLACK);
    init_pair(5, COLOR_BLACK, COLOR_WHITE);
    init_pair(6, COLOR_WHITE, COLOR_BLACK);

>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    std::wstring err_ip;
    std::string  server_ip = "127.0.0.1";
    while (true) {
        std::wstring tip = readSimpleInput(L"Enter server IP (Enter = 127.0.0.1):", err_ip);
        server_ip = to_string(tip);
        if (server_ip.empty() || server_ip == "localhost") server_ip = "127.0.0.1";

        if (!initNetwork()) { endwin(); std::cerr << "Network init error\n"; return 1; }

        struct sockaddr_in sa; memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET; sa.sin_port = htons(DEFAULT_PORT);
        if (inet_pton(AF_INET, server_ip.c_str(), &sa.sin_addr) <= 0)
        {
            err_ip = L"Invalid IP address format! Please try again."; cleanupNetwork(); continue;
        }
        sock = socket(AF_INET, SOCK_STREAM, 0);

<<<<<<< HEAD
        // Быстрое подключение с таймаутом в 2 секунды вместо блокирующего ожидания операционной системы
=======
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
        if (!connectWithTimeout(sock, (struct sockaddr*)&sa, sizeof(sa), 2))
        {
            err_ip = L"No connection to " + to_wstring(server_ip) + L". Is the server running?"; closesocket(sock); cleanupNetwork(); continue;
        }
        break;
    }

<<<<<<< HEAD
    // --- TLS шифрование ---
=======
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    SSL_load_error_strings(); OpenSSL_add_ssl_algorithms();
    client_ctx = SSL_CTX_new(TLS_client_method());
    ssl_conn = SSL_new(client_ctx);
    SSL_set_fd(ssl_conn, (int)sock);
    if (SSL_connect(ssl_conn) <= 0) {
        endwin(); SSL_free(ssl_conn); closesocket(sock); SSL_CTX_free(client_ctx);
        std::cerr << "SSL Handshake failed\n"; return 1;
    }

<<<<<<< HEAD
    // --- Экран авторизации (Никнейм) ---
=======
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    std::wstring err_user;
    while (true) {
        std::wstring wu = readSimpleInput(L"Enter your username:", err_user);
        if (wu.empty()) { err_user = L"Username cannot be empty!"; continue; }
        my_username = wu;
        std::string ru = to_string(wu);
<<<<<<< HEAD
        CLIENT_LOG = "client_" + ru + ".log";
=======
        CLIENT_LOG = "logs/client_" + ru + ".log";
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
        sendCommand(CMD_LOGIN + "|" + ru);

        char buf[BUFFER_SIZE]; memset(buf, 0, BUFFER_SIZE);
        int rb = SSL_read(ssl_conn, buf, BUFFER_SIZE - 1);
        if (rb > 0) {
            std::string resp(buf, rb);
            if (resp.find(CMD_ERROR) == 0) {
                size_t sep = resp.find('|');
                err_user = L"Error: " + to_wstring(sep != std::string::npos ? resp.substr(sep + 1) : resp);
                continue;
<<<<<<< HEAD
            }
        }
        break;
    }

    // --- Инициализация окон ---
    int my, mx; getmaxyx(stdscr, my, mx);
    header_win = newwin(1, mx, 0, 0);
    chat_win = newwin(my - 3, mx, 1, 0);
    input_win = newwin(2, mx, my - 2, 0);
    scrollok(chat_win, FALSE);

    triggerGlobalUpdate();
    std::thread recv_thread(receiveThread);

    // -----------------------------------------------------------------------
    // ГЛАВНЫЙ ЦИКЛ ОБРАБОТКИ СОБЫТИЙ
    // -----------------------------------------------------------------------
    while (running) {
        int k = read_key();
        if (k == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue; }

        // Функциональные клавиши (k < 0)
        if (k < 0) {
            int key = -k;
            if (key == KEY_RESIZE) { resizeUI(); continue; }
            if (key == KEY_BACKSPACE) {
                if (current_mode == MODE_INPUT && !current_input_string.empty())
                {
                    current_input_string.pop_back(); triggerGlobalUpdate();
                }
                continue;
            }
            if (key == KEY_F(2)) {
                if (!chat_history.empty()) {
                    current_mode = MODE_SELECT;
                    if (selected_line_idx == -1) selected_line_idx = (int)chat_history.size() - 1;
                    triggerGlobalUpdate();
                }
                continue;
            }
            if (key == 27) { // ESC
=======
            }
        }
        logMessage(CLIENT_LOG, "INFO", "User " + ru + " successfully logged in.");
        break;
    }

    int my, mx; getmaxyx(stdscr, my, mx);
    header_win = newwin(1, mx, 0, 0);
    chat_win = newwin(my - 3, mx, 1, 0);
    input_win = newwin(2, mx, my - 2, 0);
    scrollok(chat_win, FALSE);

    triggerGlobalUpdate();
    std::thread recv_thread(receiveThread);

    while (running) {
        int k = read_key();
        if (k == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue; }

        if (k < 0) {
            int key = -k;
            if (key == KEY_RESIZE) { resizeUI(); continue; }
            if (key == KEY_BACKSPACE) {
                if (current_mode == MODE_INPUT && !current_input_string.empty())
                {
                    current_input_string.pop_back(); triggerGlobalUpdate();
                }
                continue;
            }
            if (key == KEY_F(2)) {
                if (!chat_history.empty() || has_forward_buffer) {
                    current_mode = MODE_SELECT;
                    if (chat_history.empty()) {
                        selected_line_idx = -1;
                    }
                    else {
                        if (selected_line_idx == -1) selected_line_idx = (int)chat_history.size() - 1;
                    }
                    triggerGlobalUpdate();
                }
                continue;
            }
            if (key == 27) {
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
                if (current_mode == MODE_SELECT) { current_mode = MODE_INPUT; triggerGlobalUpdate(); }
                continue;
            }
            if (current_mode == MODE_SELECT) {
                int cy, cx; getmaxyx(chat_win, cy, cx); (void)cx;
                if (key == KEY_UP && selected_line_idx > 0) {
                    --selected_line_idx;
                    if (selected_line_idx < scroll_offset) scroll_offset = selected_line_idx;
                    triggerGlobalUpdate();
                }
                else if (key == KEY_DOWN && selected_line_idx < (int)chat_history.size() - 1) {
                    ++selected_line_idx;
                    if (selected_line_idx >= scroll_offset + cy) scroll_offset = selected_line_idx - cy + 1;
                    triggerGlobalUpdate();
                }
            }
            continue;
        }

<<<<<<< HEAD
        // Печатные символы и переводы строк (k > 0)
=======
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
        if (k == '\n') {
            if (current_mode == MODE_INPUT && !current_input_string.empty()) {
                handleOutboundCommand(current_input_string);
                current_input_string.clear();
                triggerGlobalUpdate();
            }
            continue;
        }
        if (current_mode == MODE_SELECT) {
            if (k == 'r' || k == 'R') {
<<<<<<< HEAD
                DisplayMessage& t = chat_history[selected_line_idx];
                if (!t.is_system) {
                    current_input_string = L"(reply to " + t.sender + L": \"" + t.text + L"\") ";
                    current_mode = MODE_INPUT; triggerGlobalUpdate();
                }
            }
            else if (k == 'f' || k == 'F') {
                DisplayMessage& t = chat_history[selected_line_idx];
=======
                if (!chat_history.empty() && selected_line_idx >= 0 && selected_line_idx < (int)chat_history.size()) {
                    DisplayMessage& t = chat_history[selected_line_idx];
                    if (!t.is_system) {
                        current_input_string = L"(reply to " + t.sender + L": \"" + t.text + L"\") ";
                        current_mode = MODE_INPUT; triggerGlobalUpdate();
                    }
                }
            }
            else if (k == 'f' || k == 'F') {
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
                if (has_forward_buffer) {
                    stateMutex.lock(); std::wstring fcp = active_chat_partner, fcg = active_group; stateMutex.unlock();
                    std::wstring fmt = L"(fwd from " + forward_buffer.sender + L"): " + forward_buffer.text;
                    pushMessageToHistory(my_username, fmt);
<<<<<<< HEAD
                    if (!fcp.empty()) sendCommand(CMD_MSG + "|" + to_string(fcp) + "|" + to_string(fmt));
                    else if (!fcg.empty()) sendCommand(CMD_GROUP_MSG + "|" + to_string(fcg) + "|" + to_string(fmt));
                    has_forward_buffer = false; current_mode = MODE_INPUT;
                    pushMessageToHistory(L"System", L"[Buffer]: Forwarded!", true, 2);
                }
                else if (!t.is_system) {
                    forward_buffer = t; has_forward_buffer = true;
                    pushMessageToHistory(L"System", L"[Buffer]: Copied. Go to chat, press F2 -> F.", true, 2);
                    current_mode = MODE_INPUT;
=======
                    if (!fcp.empty()) {
                        sendCommand(CMD_MSG + "|" + to_string(fcp) + "|" + to_string(fmt));
                        logMessage(CLIENT_LOG, "OUT", "To " + to_string(fcp) + ": " + to_string(fmt));
                    }
                    else if (!fcg.empty()) {
                        sendCommand(CMD_GROUP_MSG + "|" + to_string(fcg) + "|" + to_string(fmt));
                        logMessage(CLIENT_LOG, "G_OUT", "[" + to_string(fcg) + "] " + to_string(fmt));
                    }
                    has_forward_buffer = false; current_mode = MODE_INPUT;
                    pushMessageToHistory(L"System", L"[Buffer]: Forwarded!", true, 2);
                }
                else {
                    if (!chat_history.empty() && selected_line_idx >= 0 && selected_line_idx < (int)chat_history.size()) {
                        DisplayMessage& t = chat_history[selected_line_idx];
                        if (!t.is_system) {
                            forward_buffer = t; has_forward_buffer = true;
                            pushMessageToHistory(L"System", L"[Buffer]: Copied. Go to chat, press F2 -> F.", true, 2);
                            current_mode = MODE_INPUT;
                        }
                    }
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
                }
                triggerGlobalUpdate();
            }
            continue;
        }
<<<<<<< HEAD
        // Обычный печатный ввод (MODE_INPUT)
=======
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
        if (k >= 32) {
            current_input_string.push_back((wchar_t)k);
            triggerGlobalUpdate();
        }
    }

<<<<<<< HEAD
    // --- Завершение работы программы ---
=======
>>>>>>> 25b9a08 (Добавлена команда /users для просмотра участников группы, исправлена ошибка с Forward, улучшено логирование, добавлены сообщения о добавлении/удалении участников группы)
    if (recv_thread.joinable()) recv_thread.join();
    delwin(header_win); delwin(chat_win); delwin(input_win);
    endwin();
    if (ssl_conn) { SSL_shutdown(ssl_conn); SSL_free(ssl_conn); }
    if (client_ctx)   SSL_CTX_free(client_ctx);
    if (sock != INVALID_SOCKET) closesocket(sock);
    cleanupNetwork();
    return 0;
}