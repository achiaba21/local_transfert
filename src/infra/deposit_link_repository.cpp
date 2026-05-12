#include "ltr/infra/deposit_link_repository.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <system_error>

#include <nlohmann/json.hpp>

#include "ltr/core/logger.hpp"

namespace ltr::infra {

namespace {

nlohmann::json linkToJson(const DepositLink& link) {
    return {
        {"id",                  link.id},
        {"token",               link.token},
        {"label",               link.label},
        {"consentText",         link.consentText},
        {"maxBytesPerDeposit",  link.maxBytesPerDeposit},
        {"maxFilesPerDeposit",  link.maxFilesPerDeposit},
        {"createdAt",           link.createdAt},
        {"expiresAt",           link.expiresAt},
        {"revoked",             link.revoked},
    };
}

DepositLink linkFromJson(const nlohmann::json& j) {
    DepositLink link;
    link.id                 = j.value("id", std::string{});
    link.token              = j.value("token", std::string{});
    link.label              = j.value("label", std::string{});
    link.consentText        = j.value("consentText", std::string{});
    link.maxBytesPerDeposit = j.value("maxBytesPerDeposit", std::uint64_t{0});
    link.maxFilesPerDeposit = j.value("maxFilesPerDeposit", 0);
    link.createdAt          = j.value("createdAt", std::int64_t{0});
    link.expiresAt          = j.value("expiresAt", std::int64_t{0});
    link.revoked            = j.value("revoked", false);
    return link;
}

} // namespace

JsonDepositLinkRepository::JsonDepositLinkRepository(std::filesystem::path path)
    : path_(std::move(path)) {}

std::vector<DepositLink>
JsonDepositLinkRepository::readAllLocked() const {
    std::vector<DepositLink> out;
    std::error_code ec;
    if (!std::filesystem::exists(path_, ec)) return out;

    try {
        std::ifstream in(path_);
        const auto j = nlohmann::json::parse(in);
        const auto arr = j.value("links", nlohmann::json::array());
        out.reserve(arr.size());
        for (const auto& item : arr) {
            if (item.is_object()) out.push_back(linkFromJson(item));
        }
    } catch (const std::exception& e) {
        core::log_warn(std::string("[deposit_links] parse failed: ")
                       + e.what());
    }
    return out;
}

void JsonDepositLinkRepository::writeAllLocked(
        const std::vector<DepositLink>& links) const {
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& l : links) arr.push_back(linkToJson(l));
    nlohmann::json root;
    root["links"] = arr;

    const auto tmp = path_.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) {
            core::log_error("[deposit_links] cannot write " + tmp);
            return;
        }
        out << root.dump(2);
    }
    std::filesystem::rename(tmp, path_, ec);
    if (ec) {
        core::log_error("[deposit_links] rename failed: " + ec.message());
    }
}

std::vector<DepositLink> JsonDepositLinkRepository::loadAll() const {
    return readAllLocked();
}

void JsonDepositLinkRepository::save(const DepositLink& link) {
    auto all = readAllLocked();
    bool found = false;
    for (auto& l : all) {
        if (l.id == link.id) {
            l = link;
            found = true;
            break;
        }
    }
    if (!found) all.push_back(link);
    writeAllLocked(all);
}

void JsonDepositLinkRepository::remove(const std::string& id) {
    auto all = readAllLocked();
    all.erase(std::remove_if(all.begin(), all.end(),
        [&](const DepositLink& l) { return l.id == id; }), all.end());
    writeAllLocked(all);
}

} // namespace ltr::infra
