#pragma once

#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace ltr::core {

enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
public:
    static Logger& instance();

    void setLevel(LogLevel lvl) noexcept { level_ = lvl; }

    // V1.1.1 : écrit aussi dans un fichier de log pour analyse offline.
    // Appelé une fois au démarrage de l'app. Le chemin est logué en stdout.
    void openFile(const std::string& path);

    void log(LogLevel lvl, std::string_view msg);

    void debug(std::string_view msg) { log(LogLevel::Debug, msg); }
    void info (std::string_view msg) { log(LogLevel::Info,  msg); }
    void warn (std::string_view msg) { log(LogLevel::Warn,  msg); }
    void error(std::string_view msg) { log(LogLevel::Error, msg); }

private:
    Logger() = default;
    std::mutex    mu_;
    LogLevel      level_{LogLevel::Info};
    std::ofstream file_;
};

// Helpers globaux pour alléger le site d'appel.
inline void log_info (std::string_view m) { Logger::instance().info(m);  }
inline void log_warn (std::string_view m) { Logger::instance().warn(m);  }
inline void log_error(std::string_view m) { Logger::instance().error(m); }
inline void log_debug(std::string_view m) { Logger::instance().debug(m); }

} // namespace ltr::core
