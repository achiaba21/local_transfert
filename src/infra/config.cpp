#include "ltr/infra/config.hpp"

#include "ltr/core/logger.hpp"

#include <array>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <system_error>

#include <nlohmann/json.hpp>

#ifdef _WIN32
  #include <windows.h>
  #include <lmcons.h>
#else
  #include <unistd.h>
  #include <pwd.h>
  #include <sys/types.h>
#endif

namespace ltr::infra {

namespace {

std::filesystem::path homeDir() {
#ifdef _WIN32
    if (const char* h = std::getenv("USERPROFILE")) return h;
    if (const char* h = std::getenv("HOMEDRIVE")) {
        const char* p = std::getenv("HOMEPATH");
        if (p) return std::filesystem::path(h) / p;
    }
    return std::filesystem::current_path();
#else
    if (const char* h = std::getenv("HOME")) return h;
    if (const passwd* pw = getpwuid(getuid())) return pw->pw_dir;
    return "/tmp";
#endif
}

std::string currentUserName() {
#ifdef _WIN32
    char buf[UNLEN + 1];
    DWORD sz = sizeof(buf);
    if (GetUserNameA(buf, &sz)) return std::string(buf, sz - 1);
    return "user";
#else
    if (const passwd* pw = getpwuid(getuid())) return pw->pw_name;
    if (const char* u = std::getenv("USER")) return u;
    return "user";
#endif
}

// V1.1.1 : détecte si une chaîne ressemble à une adresse IPv4 (le gethostname
// peut retourner l'IP sur macOS si le DNS réseau est configuré ainsi).
bool looksLikeIp(const std::string& s) {
    int dots = 0;
    for (char c : s) {
        if (c == '.') ++dots;
        else if (!(c >= '0' && c <= '9')) return false;
    }
    return dots == 3;
}

std::string currentHostName() {
#ifdef _WIN32
    char buf[256];
    DWORD sz = sizeof(buf);
    if (GetComputerNameA(buf, &sz)) return std::string(buf);
    return "host";
#else
    // V1.1.1 : sur macOS, `scutil --get ComputerName` donne le nom convivial
    // (ex. "MacBook Pro"), plus stable que gethostname() qui peut renvoyer
    // une IP selon la config réseau. On l'essaie en premier sur __APPLE__.
  #ifdef __APPLE__
    {
        FILE* fp = popen("scutil --get ComputerName 2>/dev/null", "r");
        if (fp) {
            char buf[256] = {0};
            const auto n = fread(buf, 1, sizeof(buf) - 1, fp);
            pclose(fp);
            if (n > 0) {
                std::string s(buf, n);
                while (!s.empty() &&
                       (s.back() == '\n' || s.back() == ' ' || s.back() == '\r'))
                    s.pop_back();
                if (!s.empty() && !looksLikeIp(s)) return s;
            }
        }
    }
  #endif
    char buf[256];
    if (gethostname(buf, sizeof(buf)) == 0) {
        std::string s(buf);
        if (!looksLikeIp(s)) return s;
    }
    return "host";
#endif
}

} // namespace

std::string generateUuidV4() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<std::uint64_t> dist;

    std::array<std::uint8_t, 16> b{};
    std::uint64_t a = dist(rng), c = dist(rng);
    for (int i = 0; i < 8; ++i) b[i]     = static_cast<std::uint8_t>(a >> (8 * i));
    for (int i = 0; i < 8; ++i) b[i + 8] = static_cast<std::uint8_t>(c >> (8 * i));

    b[6] = (b[6] & 0x0F) | 0x40; // version 4
    b[8] = (b[8] & 0x3F) | 0x80; // variant

    std::ostringstream os;
    os << std::hex << std::setfill('0');
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) os << '-';
        os << std::setw(2) << static_cast<int>(b[i]);
    }
    return os.str();
}

std::string detectPlatform() {
#if defined(__APPLE__)
    return "macOS";
#elif defined(_WIN32)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

std::string defaultDeviceName() {
    return currentUserName() + "@" + currentHostName();
}

namespace {

// V1.1.1 : détecte les deviceNames figés avec une IP (legacy bug). Si on
// trouve "@<digits>.<digits>..." après l'arobase, on régénère pour avoir
// le hostname courant. Évite l'incohérence entre le nom affiché et l'IP
// réelle servie dans la SharePanel.
bool deviceNameContainsIp(const std::string& name) {
    const auto at = name.find('@');
    if (at == std::string::npos) return false;
    const auto after = name.substr(at + 1);
    // Forme stricte : xxx.xxx.xxx.xxx
    int dotCount = 0;
    for (char c : after) {
        if (c == '.') ++dotCount;
        else if (!(c >= '0' && c <= '9')) return false;
    }
    return dotCount == 3;
}

} // namespace

std::filesystem::path Config::configDir() {
    return configPath().parent_path();
}

std::filesystem::path Config::configPath() {
#ifdef _WIN32
    std::filesystem::path base;
    if (const char* appdata = std::getenv("APPDATA")) base = appdata;
    else base = homeDir();
    return base / "LocalTransfer" / "config.json";
#elif defined(__APPLE__)
    return homeDir() / "Library" / "Application Support"
         / "LocalTransfer" / "config.json";
#else
    return homeDir() / ".config" / "local-transfer" / "config.json";
#endif
}

Config Config::loadOrCreate() {
    const auto path = configPath();
    Config cfg;

    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        try {
            std::ifstream in(path);
            nlohmann::json j = nlohmann::json::parse(in);
            cfg.deviceId    = j.value("deviceId",    std::string{});
            cfg.deviceName  = j.value("deviceName",  std::string{});
            cfg.platform    = j.value("platform",    std::string{});
            cfg.downloadDir = j.value("downloadDir", std::string{});
            cfg.sharePanelCollapsed = j.value("sharePanelCollapsed", false);
            cfg.autoRetryCount = j.value("autoRetryCount", 2);
            cfg.resumeSidecarTtlHours = j.value("resumeSidecarTtlHours", 24);
            cfg.webAnnounceTimeoutSec = j.value("webAnnounceTimeoutSec", 300);
        } catch (const std::exception& e) {
            core::log_warn(std::string("Config corrompu, régénération : ") + e.what());
        }
    }

    bool dirty = false;
    if (cfg.deviceId.empty())    { cfg.deviceId = generateUuidV4(); dirty = true; }
    if (cfg.platform.empty())    { cfg.platform = detectPlatform(); dirty = true; }
    if (cfg.deviceName.empty())  { cfg.deviceName = defaultDeviceName(); dirty = true; }
    // V1.1.1 : migration des anciens deviceNames figés avec IP.
    if (deviceNameContainsIp(cfg.deviceName)) {
        const auto oldName = cfg.deviceName;
        cfg.deviceName = defaultDeviceName();
        core::log_info("Config: deviceName migré '" + oldName + "' → '"
                       + cfg.deviceName + "'");
        dirty = true;
    }
    if (cfg.downloadDir.empty()) {
        cfg.downloadDir = homeDir() / "Downloads" / "LocalTransfer";
        dirty = true;
    }

    std::filesystem::create_directories(cfg.downloadDir, ec);

    if (dirty) cfg.save();
    return cfg;
}

void Config::save() const {
    const auto path = configPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    nlohmann::json j;
    j["deviceId"]    = deviceId;
    j["deviceName"]  = deviceName;
    j["platform"]    = platform;
    j["downloadDir"] = downloadDir.string();
    j["sharePanelCollapsed"] = sharePanelCollapsed;
    j["autoRetryCount"] = autoRetryCount;
    j["resumeSidecarTtlHours"] = resumeSidecarTtlHours;
    j["webAnnounceTimeoutSec"] = webAnnounceTimeoutSec;

    std::ofstream out(path);
    out << j.dump(2);
}

} // namespace ltr::infra
