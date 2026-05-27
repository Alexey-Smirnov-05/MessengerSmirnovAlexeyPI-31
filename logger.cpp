#include "logger.h"
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <mutex>

static std::mutex logMutex;

void logMessage(const std::string& filename, const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    std::ofstream file(filename, std::ios::app);
    if (!file.is_open()) return;

    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_info = std::localtime(&now_time);

    file << "[" << std::put_time(tm_info, "%Y-%m-%d %H:%M:%S") << "] "
        << level << ": " << message << std::endl;
}