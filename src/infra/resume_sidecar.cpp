#include "ltr/infra/resume_sidecar.hpp"

#include "ltr/core/logger.hpp"

#include <nlohmann/json.hpp>

#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace ltr::infra {

namespace {

using json = nlohmann::json;

std::string statusToStr(FileResumeStatus s) {
    switch (s) {
        case FileResumeStatus::NotStarted: return "not_started";
        case FileResumeStatus::Partial:    return "partial";
        case FileResumeStatus::Done:       return "done";
    }
    return "not_started";
}

FileResumeStatus statusFromStr(const std::string& s) {
    if (s == "done") return FileResumeStatus::Done;
    if (s == "partial") return FileResumeStatus::Partial;
    return FileResumeStatus::NotStarted;
}

std::string timePointToIso8601(std::chrono::system_clock::time_point tp) {
    const auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return os.str();
}

std::chrono::system_clock::time_point iso8601ToTimePoint(const std::string& s) {
    std::tm tm{};
    std::istringstream is(s);
    is >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (is.fail()) return {};
#if defined(_WIN32)
    const auto t = _mkgmtime(&tm);
#else
    const auto t = timegm(&tm);
#endif
    return std::chrono::system_clock::from_time_t(t);
}

// Écrit un JSON atomiquement : write vers .tmp, rename final.
bool writeJsonAtomic(const std::filesystem::path& target, const json& j) {
    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(), ec);

    const auto tmp = target.string() + ".tmp";
    {
        std::ofstream out(tmp);
        if (!out) {
            core::log_error("writeJsonAtomic: cannot open " + tmp);
            return false;
        }
        out << j.dump(2);
    }
    std::filesystem::rename(tmp, target, ec);
    if (ec) {
        core::log_error("writeJsonAtomic: rename failed " + ec.message());
        return false;
    }
    return true;
}

} // namespace

// =========================================================================
// Sidecar receiver
// =========================================================================

std::filesystem::path sidecarPath(const std::filesystem::path& downloadDir,
                                    const std::string& sessionId) {
    return downloadDir / ("ltr-resume-" + sessionId + ".json");
}

bool writeSidecar(const std::filesystem::path& downloadDir, const Sidecar& s) {
    json j;
    j["sessionId"]      = s.sessionId;
    j["senderDeviceId"] = s.senderDeviceId;
    j["senderName"]     = s.senderName;
    j["createdAt"]      = timePointToIso8601(s.createdAt);
    j["lastUpdateAt"]   = timePointToIso8601(s.lastUpdateAt);

    j["files"] = json::array();
    for (const auto& f : s.files) {
        json jf;
        jf["relativePath"]  = f.relativePath;
        jf["status"]        = statusToStr(f.status);
        jf["expectedSize"]  = f.expectedSize;
        jf["bytesReceived"] = f.bytesReceived;
        jf["sha256Prefix"]  = f.sha256Prefix;
        if (!f.partialPath.empty()) jf["partialPath"] = f.partialPath;
        j["files"].push_back(std::move(jf));
    }

    return writeJsonAtomic(sidecarPath(downloadDir, s.sessionId), j);
}

std::optional<Sidecar> readSidecar(const std::filesystem::path& downloadDir,
                                    const std::string& sessionId) {
    const auto p = sidecarPath(downloadDir, sessionId);
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) return std::nullopt;

    Sidecar s;
    try {
        std::ifstream in(p);
        json j = json::parse(in);
        s.sessionId      = j.value("sessionId", std::string{});
        s.senderDeviceId = j.value("senderDeviceId", std::string{});
        s.senderName     = j.value("senderName", std::string{});
        if (j.contains("createdAt"))
            s.createdAt = iso8601ToTimePoint(j["createdAt"].get<std::string>());
        if (j.contains("lastUpdateAt"))
            s.lastUpdateAt = iso8601ToTimePoint(
                j["lastUpdateAt"].get<std::string>());

        if (s.sessionId != sessionId) {
            core::log_warn("readSidecar: sessionId mismatch in " + p.string());
            std::filesystem::remove(p, ec);
            return std::nullopt;
        }

        for (const auto& jf : j.value("files", json::array())) {
            SidecarFileState f;
            f.relativePath  = jf.value("relativePath", std::string{});
            f.status        = statusFromStr(jf.value("status", std::string{"not_started"}));
            f.expectedSize  = jf.value("expectedSize", std::uint64_t{0});
            f.bytesReceived = jf.value("bytesReceived", std::uint64_t{0});
            f.sha256Prefix  = jf.value("sha256Prefix", std::string{});
            f.partialPath   = jf.value("partialPath", std::string{});
            s.files.push_back(std::move(f));
        }
    } catch (const std::exception& e) {
        core::log_warn(std::string("readSidecar: corrompu, suppression : ")
                       + e.what());
        std::filesystem::remove(p, ec);
        return std::nullopt;
    }
    return s;
}

void deleteSidecar(const std::filesystem::path& downloadDir,
                    const std::string& sessionId) {
    std::error_code ec;
    std::filesystem::remove(sidecarPath(downloadDir, sessionId), ec);
}

int purgeOldSidecars(const std::filesystem::path& downloadDir, int ttlHours) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(downloadDir, ec)) return 0;

    const auto now = std::chrono::system_clock::now();
    const auto maxAge = std::chrono::hours(ttlHours);

    int removed = 0;
    for (const auto& entry : fs::directory_iterator(downloadDir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        const auto name = entry.path().filename().string();
        if (name.rfind("ltr-resume-", 0) != 0) continue;
        if (entry.path().extension() != ".json") continue;

        const auto ftime = fs::last_write_time(entry, ec);
        if (ec) continue;

        // fs::file_time_type → system_clock via duration_cast sur la diff
        // entre now de chacun (approximation suffisante pour un TTL horaire).
        const auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now()
            + std::chrono::system_clock::now());

        if (now - sctp > maxAge) {
            fs::remove(entry, ec);
            if (!ec) {
                ++removed;
                core::log_info("[resume] sidecar purgé (TTL) : " + name);
            }
        }
    }
    return removed;
}

// =========================================================================
// Pending sessions côté sender
// =========================================================================

namespace {
std::filesystem::path pendingSessionsPath(
    const std::filesystem::path& configDir) {
    return configDir / "pending-sessions.json";
}
} // namespace

std::vector<PendingSession> loadPendingSessions(
    const std::filesystem::path& configDir) {
    const auto p = pendingSessionsPath(configDir);
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) return {};

    std::vector<PendingSession> out;
    try {
        std::ifstream in(p);
        json j = json::parse(in);
        if (!j.is_array()) return {};
        for (const auto& je : j) {
            PendingSession ps;
            ps.sessionId    = je.value("sessionId", std::string{});
            ps.peerId       = je.value("peerId", std::string{});
            ps.peerName     = je.value("peerName", std::string{});
            ps.peerIp       = je.value("peerIp", std::string{});
            ps.peerTcpPort  = static_cast<std::uint16_t>(
                je.value("peerTcpPort", 0));
            ps.pinCode      = je.value("pinCode", std::string{});
            for (const auto& sp : je.value("sourcePaths", json::array())) {
                ps.sourcePaths.emplace_back(sp.get<std::string>());
            }
            ps.totalBytes         = je.value("totalBytes", std::uint64_t{0});
            ps.bytesTransferred   = je.value("bytesTransferred", std::uint64_t{0});
            ps.retryAttempts      = je.value("retryAttempts", 0);
            ps.lastErrorCategory  = je.value("lastErrorCategory", std::string{});
            if (je.contains("createdAt"))
                ps.createdAt = iso8601ToTimePoint(
                    je["createdAt"].get<std::string>());
            if (!ps.sessionId.empty()) out.push_back(std::move(ps));
        }
    } catch (const std::exception& e) {
        core::log_warn(std::string("loadPendingSessions corrompu : ")
                       + e.what());
        std::filesystem::remove(p, ec);
        return {};
    }
    return out;
}

bool savePendingSessions(const std::filesystem::path& configDir,
                          const std::vector<PendingSession>& sessions) {
    json j = json::array();
    for (const auto& ps : sessions) {
        json je;
        je["sessionId"]    = ps.sessionId;
        je["peerId"]       = ps.peerId;
        je["peerName"]     = ps.peerName;
        je["peerIp"]       = ps.peerIp;
        je["peerTcpPort"]  = ps.peerTcpPort;
        je["pinCode"]      = ps.pinCode;
        je["sourcePaths"]  = json::array();
        for (const auto& p : ps.sourcePaths) {
            je["sourcePaths"].push_back(p.string());
        }
        je["totalBytes"]        = ps.totalBytes;
        je["bytesTransferred"]  = ps.bytesTransferred;
        je["retryAttempts"]     = ps.retryAttempts;
        je["lastErrorCategory"] = ps.lastErrorCategory;
        je["createdAt"]         = timePointToIso8601(ps.createdAt);
        j.push_back(std::move(je));
    }
    return writeJsonAtomic(pendingSessionsPath(configDir), j);
}

} // namespace ltr::infra
