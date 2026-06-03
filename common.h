#ifndef COMMON_H
#define COMMON_H

#include <string>
#include <iostream>

// --- КРОССПЛАТФОРМЕННАЯ НАСТРОЙКА СЕТИ ---
#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS             // Отключение предупреждений об устаревших функциях Winsock
#include <winsock2.h>                               // Основной заголовочный файл Winsock API для Windows
#include <ws2tcpip.h>                               // Поддержка современных сетевых протоколов (включая inet_pton)
#pragma comment(lib, "ws2_32.lib")                  // Автоматическая линковка системной библиотеки Winsock
#else
#include <sys/socket.h>                             // Основные структуры и функции для работы с сокетами в Unix/Linux
#include <netinet/in.h>                             // Структуры для работы с интернет-протоколами (sockaddr_in)
#include <arpa/inet.h>                              // Функции преобразования сетевых адресов (inet_addr, inet_pton)
#include <unistd.h>                                 // Системные вызовы Unix (включая функцию close)

#define SOCKET int                                  // Определение дескриптора сокета для совместимости с Windows
#define INVALID_SOCKET -1                           // Константа некорректного сокета для Unix-систем
#define SOCKET_ERROR -1                             // Константа кода сетевой ошибки для Unix-систем
#define closesocket close                           // Переопределение закрытия сокета под Unix-стиль
#endif

// --- ГЛОБАЛЬНЫЕ СЕТЕВЫЕ НАСТРОЙКИ ---
const int DEFAULT_PORT = 8888;                      // Сетевой порт по умолчанию для входящих TLS-подключений
const int BUFFER_SIZE = 4096;                       // Размер буфера (в байтах) для чтения и записи в сокет

// --- СЕТЕВЫЕ КОМАНДЫ (ПРОТОКОЛ ВЗАИМОДЕЙСТВИЯ КЛИЕНТ-СЕРВЕР) ---
const std::string CMD_LOGIN = "LOGIN";               // Запрос от клиента на авторизацию под уникальным именем
const std::string CMD_QUIT = "QUIT";                 // Запрос от клиента на корректный выход и закрытие сессии
const std::string CMD_MSG = "MSG";                   // Отправка клиентом исходящего приватного (личного) сообщения
const std::string CMD_OK = "OK";                     // Серверный ответ об успешном выполнении запрошенной операции
const std::string CMD_ERROR = "ERROR";               // Серверный ответ об ошибке выполнения (с текстом причины)
const std::string CMD_INMSG = "INMSG";               // Входящее приватное сообщение, пересылаемое сервером получателю
const std::string CMD_GROUP_JOIN = "GJOIN";         // Запрос на создание новой или вход в существующую группу
const std::string CMD_GROUP_MSG = "GMSG";           // Передача текстового сообщения внутри определенной группы
const std::string CMD_GROUP_ADD = "GADD";           // Админ-команда: пригласить/добавить пользователя в закрытую группу
const std::string CMD_GROUP_KICK = "GKICK";         // Админ-команда: исключить (кикнуть) участника из списка доступа группы
const std::string CMD_GROUP_DEL = "GDEL";           // Админ-команда: полное удаление группы и её конфигурационного бэкапа
const std::string CMD_GROUP_NOTIFY = "GNOTIFY";     // Серверное уведомление клиенту о событиях группы (KICKED, DELETED, ADDED)

// --- КОМАНДЫ РАБОТЫ С ИСТОРИЕЙ И ОНЛАЙН-СТАТУСАМИ ---
const std::string CMD_REQ_HISTORY = "REQHIST";       // Запрос клиенту на загрузку архивных логов переписки (PM или GROUP)
const std::string CMD_HIST_LINE = "HISTLINE";       // Серверная отправка одной строки из файла истории сообщений
const std::string CMD_REQ_ONLINE = "REQONLINE";     // Запрос клиента на получение списка пользователей, находящихся в сети
const std::string CMD_ONLINE_LIST = "ONLINELIST";   // Ответ сервера, содержащий перечень активных логинов через запятую

// --- СЕРВИСНЫЕ И АДМИНИСТРАТИВНЫЕ КОМАНДЫ ---
const std::string CMD_CLEAR = "CLEAR";               // Запрос на безвозвратную очистку файлов истории текущего чата

// --- ИНИЦИАЛИЗАЦИЯ И ОЧИСТКА СЕТЕВЫХ СУБСИСТЕМ ---
inline bool initNetwork() {
#ifdef _WIN32
    WSADATA wsaData;                                // Структура для хранения параметров реализации Windows Sockets
    return WSAStartup(MAKEWORD(2, 2), &wsaData) == 0; // Инициализация сокетов версии 2.2 для Windows среды
#else
    return true;                                    // В POSIX/Unix системах предварительная инициализация не требуется
#endif
}

inline void cleanupNetwork() {
#ifdef _WIN32
    WSACleanup();                                   // Освобождение ресурсов и деинициализация сокетов в Windows
#endif
}

#endif