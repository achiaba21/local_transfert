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

    if (file_.is_open()) {
        file_ << '[' << ts << "] [" << lbl << "] " << msg << "\n"
              << std::flush;
    }
}

namespace {

// Échappe une chaîne pour JSON (simple : \", \\, \n, \r, \t).
// Cap à 200 caractères pour éviter l'inflation des logs.
std::string jsonEscape(std::string_view s) {
    constexpr std::size_t kCap = 200;
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    std::size_t n = 0;
    for (char c : s) {
        if (n++ >= kCap) { out += "...\""; return out; }
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(c);
                }
        }
    }
    out.push_back('"');
    return out;
}

} // namespace

void Logger::logEvent(LogLevel lvl, std::string_view name,
                     std::initializer_list<
                       std::pair<std::string_view, std::string>> fields) {
    if (static_cast<int>(lvl) < static_cast<int>(level_)) return;
    std::lock_guard<std::mutex> lock(mu_);
    const auto ts = timestamp();
    const char* lbl = levelLabel(lvl);

    std::string line;
    if (format_ == LogFormat::Json) {
        line.reserve(128 + fields.size() * 32);
        line += "{\"ts\":";
        line += jsonEscape(ts);
        line += ",\"level\":\"";
        line += lbl;
        line += "\",\"event\":";
        line += jsonEscape(name);
        line += ",\"fields\":{";
        bool first = true;
        for (const auto& [k, v] : fields) {
            if (!first) line += ',';
            first = false;
            line += jsonEscape(k);
            line += ':';
            line += jsonEscape(v);
        }
        line += "}}";
    } else {
        line.reserve(64 + fields.size() * 16);
        line += '[';
        line += ts;
        line += "] [";
        line += lbl;
        line += "] [";
        line += name;
        line += ']';
        for (const auto& [k, v] : fields) {
            line += ' ';
            line += k;
            line += '=';
            line += v;
        }
    }

    auto& out = (lvl >= LogLevel::Warn) ? std::cerr : std::cout;
    out << line << std::endl;
    if (file_.is_open()) {
        file_ << line << "\n" << std::flush;
    }
}

} // namespace ltr::core
