#include "ltr/web/download_ticket_store.hpp"

#include <random>

#include "ltr/core/types.hpp"

namespace ltr::web {

namespace {

std::string makeTicketId() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::string id;
    id.reserve(32);
    for (int i = 0; i < 16; ++i) {
        const auto b = static_cast<std::uint8_t>(rng() & 0xFF);
        constexpr char kHex[] = "0123456789abcdef";
        id.push_back(kHex[(b >> 4) & 0xF]);
        id.push_back(kHex[b & 0xF]);
    }
    return id;
}

} // namespace

std::string DownloadTicketStore::issue(const std::string& sessionToken,
                                       const std::string& sessionId,
                                       const std::filesystem::path& path,
                                       const std::string& displayName,
                                       std::uint64_t size) {
    DownloadTicket t;
    t.id           = makeTicketId();
    t.sessionToken = sessionToken;
    t.sessionId    = sessionId;
    t.kind         = TicketKind::File;
    t.path         = path;
    t.displayName  = displayName;
    t.size         = size;
    t.expiresAt    = std::chrono::steady_clock::now() + core::kDownloadTicketTtl;

    std::lock_guard<std::mutex> lock(mu_);
    const auto id = t.id;
    tickets_.emplace(id, std::move(t));
    return id;
}

std::string DownloadTicketStore::issueStreamingZip(
    const std::string& sessionToken,
    const std::string& sessionId,
    std::vector<ZipEntry> entries,
    const std::string& displayName,
    std::uint64_t zipSize) {
    DownloadTicket t;
    t.id           = makeTicketId();
    t.sessionToken = sessionToken;
    t.sessionId    = sessionId;
    t.kind         = TicketKind::StreamingZip;
    t.zipEntries   = std::move(entries);
    t.displayName  = displayName;
    t.size         = zipSize;
    t.expiresAt    = std::chrono::steady_clock::now() + core::kDownloadTicketTtl;

    std::lock_guard<std::mutex> lock(mu_);
    const auto id = t.id;
    tickets_.emplace(id, std::move(t));
    return id;
}

std::optional<DownloadTicket> DownloadTicketStore::get(
    const std::string& ticketId) {
    std::lock_guard<std::mutex> lock(mu_);
    const auto it = tickets_.find(ticketId);
    if (it == tickets_.end()) return std::nullopt;

    const auto now = std::chrono::steady_clock::now();
    if (now > it->second.expiresAt) {
        tickets_.erase(it);
        return std::nullopt;
    }
    return it->second;
}

std::vector<DownloadTicket> DownloadTicketStore::evictExpired() {
    const auto now = std::chrono::steady_clock::now();
    std::vector<DownloadTicket> expired;
    std::lock_guard<std::mutex> lock(mu_);
    for (auto it = tickets_.begin(); it != tickets_.end(); ) {
        if (now > it->second.expiresAt) {
            expired.push_back(it->second);
            it = tickets_.erase(it);
        } else {
            ++it;
        }
    }
    return expired;
}

std::optional<DownloadTicket> DownloadTicketStore::peek(
    const std::string& ticketId) const {
    std::lock_guard<std::mutex> lock(mu_);
    const auto it = tickets_.find(ticketId);
    if (it == tickets_.end()) return std::nullopt;
    if (std::chrono::steady_clock::now() > it->second.expiresAt) return std::nullopt;
    return it->second;
}

std::vector<DownloadTicket> DownloadTicketStore::listBySession(
    const std::string& sessionToken) const {
    std::vector<DownloadTicket> out;
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto& [id, tkt] : tickets_) {
        if (tkt.sessionToken == sessionToken && now <= tkt.expiresAt) {
            out.push_back(tkt);
        }
    }
    return out;
}

} // namespace ltr::web
