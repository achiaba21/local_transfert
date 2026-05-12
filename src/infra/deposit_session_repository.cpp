#include "ltr/infra/deposit_session_repository.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <system_error>

#include <nlohmann/json.hpp>

#include "ltr/core/logger.hpp"

namespace ltr::infra {

namespace {

nlohmann::json fileToJson(const DepositSessionFile& f) {
    return {
        {"name",       f.name},
        {"size",       f.size},
        {"sha256",     f.sha256},
        {"storedPath", f.storedPath},
    };
}

DepositSessionFile fileFromJson(const nlohmann::json& j) {
    DepositSessionFile f;
    f.name       = j.value("name", std::string{});
    f.size       = j.value("size", std::uint64_t{0});
    f.sha256     = j.value("sha256", std::string{});
    f.storedPath = j.value("storedPath", std::string{});
    return f;
}

nlohmann::json sessionToJson(const DepositSession& s) {
    nlohmann::json files = nlohmann::json::array();
    for (const auto& f : s.files) files.push_back(fileToJson(f));
    return {
        {"id",              s.id},
        {"linkId",          s.linkId},
        {"depositorName",   s.depositorName},
        {"consentAccepted", s.consentAccepted},
        {"startedAt",       s.startedAt},
        {"finishedAt",      s.finishedAt},
        {"status",          depositSessionStatusToStr(s.status)},
        {"files",           files},
        {"totalBytes",      s.totalBytes},
    };
}

DepositSession sessionFromJson(const nlohmann::json& j) {
    DepositSession s;
    s.id              = j.value("id", std::string{});
    s.linkId          = j.value("linkId", std::string{});
    s.depositorName   = j.value("depositorName", std::string{});
    s.consentAccepted = j.value("consentAccepted", false);
    s.startedAt       = j.value("startedAt", std::int64_t{0});
    s.finishedAt      = j.value("finishedAt", std::int64_t{0});
    s.status          = depositSessionStatusFromStr(
        j.value("status", std::string{"open"}));
    s.totalBytes      = j.value("totalBytes", std::uint64_t{0});
    for (const auto& fj : j.value("files", nlohmann::json::array())) {
        if (fj.is_object()) s.files.push_back(fileFromJson(fj));
    }
    return s;
}

} // namespace

JsonDepositSessionRepository::JsonDepositSessionRepository(
        std::filesystem::path path)
    : path_(std::move(path)) {}

std::vector<DepositSession>
JsonDepositSessionRepository::readAllLocked() const {
    std::vector<DepositSession> out;
    std::error_code ec;
    if (!std::filesystem::exists(path_, ec)) return out;

    try {
        std::ifstream in(path_);
        const auto j = nlohmann::json::parse(in);
        const auto arr = j.value("sessions", nlohmann::json::array());
        out.reserve(arr.size());
        for (const auto& item : arr) {
            if (item.is_object()) out.push_back(sessionFromJson(item));
        }
    } catch (const std::exception& e) {
        core::log_warn(std::string("[deposit_sessions] parse failed: ")
                       + e.what());
    }
    return out;
}

void JsonDepositSessionRepository::writeAllLocked(
        const std::vector<DepositSession>& sessions) const {
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& s : sessions) arr.push_back(sessionToJson(s));
    nlohmann::json root;
    root["sessions"] = arr;

    const auto tmp = path_.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            core::log_error("[deposit_sessions] cannot write " + tmp);
            return;
        }
        out << root.dump(2);
    }
    std::filesystem::rename(tmp, path_, ec);
    if (ec) {
        core::log_error("[deposit_sessions] rename failed: " + ec.message());
    }
}

std::vector<DepositSession>
JsonDepositSessionRepository::loadAll() const {
    return readAllLocked();
}

std::optional<DepositSession>
JsonDepositSessionRepository::findById(const std::string& id) const {
    if (id.empty()) return std::nullopt;
    for (const auto& s : readAllLocked()) {
        if (s.id == id) return s;
    }
    return std::nullopt;
}

void JsonDepositSessionRepository::save(const DepositSession& session) {
    auto all = readAllLocked();
    bool found = false;
    for (auto& s : all) {
        if (s.id == session.id) {
            s = session;
            found = true;
            break;
        }
    }
    if (!found) all.push_back(session);
    writeAllLocked(all);
}

void JsonDepositSessionRepository::remove(const std::string& id) {
    auto all = readAllLocked();
    all.erase(std::remove_if(all.begin(), all.end(),
        [&](const DepositSession& s) { return s.id == id; }), all.end());
    writeAllLocked(all);
}

} // namespace ltr::infra
