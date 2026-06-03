#include "logger.h"
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>

// Мьютекс для синхронизации потоков при записи в один файл
static std::mutex logMutex;

// Потокобезопасная запись события в лог-файл
void logMessage(const std::string& filename, const std::string& level, const std::string& message) {
    // Автоматическая блокировка мьютекса (защита от состояния гонки)
    std::lock_guard<std::mutex> lock(logMutex);

    // Открытие файла в режиме добавления (append)
    std::ofstream file(filename, std::ios::app);
    if (!file.is_open()) return;

    // Получение и форматирование текущего времени
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_info = std::localtime(&now_time);

    // Запись в формате: [ГГГГ-ММ-ДД ЧЧ:ММ:СС] УРОВЕНЬ: Сообщение
    file << "[" << std::put_time(tm_info, "%Y-%m-%d %H:%M:%S") << "] "
        << level << ": " << message << std::endl;
}