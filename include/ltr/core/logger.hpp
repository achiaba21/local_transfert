#pragma once

#include <fstream>
#include <initializer_list>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ltr::core {

enum class LogLevel  { Debug, Info, Warn, Error };
// V1.5 — Sprint Hardening : format de log toggleable.
enum class LogFormat { Text, Json };

class Logger {
public:
    static Logger& instance();

    void setLevel(LogLevel lvl) noexcept { level_ = lvl; }

    // V1.5 — toggle Text/Json. Le format JSON produit une ligne par
    // log : {"ts","level","msg" | "event","fields":{...}}.
    void setFormat(LogFormat fmt) noexcept { format_ = fmt; }

    // V1.1.1 : écrit aussi dans un fichier de log pour analyse offline.
    void openFile(const std::string& path);

    void log(LogLevel lvl, std::string_view msg);

    // V1.5 — log structuré avec un nom d'évènement + champs k=v.
    // En mode Text, formaté `[event] k=v k=v` en clair.
    // En mode Json, ligne JSON complète {ts,level,event,fields:{}}.
    // Cap par valeur 200 chars pour éviter l'inflation.
    void logEvent(LogLevel lvl, std::string_view name,
                  std::initializer_list<
                    std::pair<std::string_view, std::string>> fields);

    void debug(std::string_view msg) { log(LogLevel::Debug, msg); }
    void info (std::string_view msg) { log(LogLevel::Info,  msg); }
    void warn (std::string_view msg) { log(LogLevel::Warn,  msg); }
    void error(std::string_view msg) { log(LogLevel::Error, msg); }

private:
    Logger() = default;
    std::mutex    mu_;
    LogLevel      level_ {LogLevel::Info};
    LogFormat     format_{LogFormat::Text};
    std::ofstream file_;
};

// Helpers globaux pour alléger le site d'appel.
inline void log_info (std::string_view m) { Logger::instance().info(m);  }
inline void log_warn (std::string_view m) { Logger::instance().warn(m);  }
inline void log_error(std::string_view m) { Logger::instance().error(m); }
inline void log_debug(std::string_view m) { Logger::instance().debug(m); }

// V1.5 — Sprint Hardening : log structuré avec champs k=v.
inline void log_event(LogLevel lvl, std::string_view name,
                      std::initializer_list<
                        std::pair<std::string_view, std::string>> fields) {
    Logger::instance().logEvent(lvl, name, fields);
}

} // namespace ltr::core
