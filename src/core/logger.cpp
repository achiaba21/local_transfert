#include "ltr/core/logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace ltr::core {

namespace {

const char* levelLabel(LogLevel l) noexcept {
    switch (l) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

std::string timestamp() {
    const auto now  = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream os;
    os << std::put_time(&tm, "%H:%M:%S");
    return os.str();
}

} // namespace

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

void Logger::openFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mu_);
    file_.open(path, std::ios::out | std::ios::app);
    if (file_) {
        file_ << "\n=== LocalTransfer — nouveau run " << timestamp()
              << " ===\n" << std::flush;
    }
}

void Logger::log(LogLevel lvl, std::string_view msg) {
    if (static_cast<int>(lvl) < static_cast<int>(level_)) return;
    std::lock_guard<std::mutex> lock(mu_);
    const auto ts = timestamp();
    const char* lbl = levelLabel(lvl);

    auto& out = (lvl >= LogLevel::Warn) ? std::cerr : std::cout;
    out << '[' << ts << "] [" << lbl << "] " << msg << std::endl;

    // V1.1.1 : duplique dans le fichier de log si ouvert.
    if (file_.is_open()) {
        file_ << '[' << ts << "] [" << lbl << "] " << msg << "\n"
              << std::flush;
    }
}

} // namespace ltr::core
