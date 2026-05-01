#include "ltr/web/web_session_store.hpp"

#include <random>

#include "ltr/core/types.hpp"
#include "ltr/domain/device.hpp"
#include "ltr/web/display_name.hpp"

namespace ltr::web {

namespace {

// Détecte la plateforme et un nom court d'agent à partir du User-Agent HTTP.
struct ParsedAgent {
    std::string platform;   // "iOS", "Android", "macOS", "Windows", "Linux", "Other"
    std::string agentName;  // "Safari", "Chrome", "Firefox", "Edge", "Other"
};

ParsedAgent parseUserAgent(const std::string& ua) {
    ParsedAgent p{"Other", "Other"};

    auto contains = [&](const char* s) {
        return ua.find(s) != std::string::npos;
    };

    if (contains("iPhone") || contains("iPad") || contains("iOS")) p.platform = "iOS";
    else if (contains("Android"))                                  p.platform = "Android";
    else if (contains("Mac OS X") || contains("Macintosh"))        p.platform = "macOS";
    else if (contains("Windows"))                                  p.platform = "Windows";
    else if (contains("Linux"))                                    p.platform = "Linux";

    // Ordre important : Edg avant Chrome, Chrome avant Safari (Chrome contient "Safari").
    if (contains("Edg/") || contains("Edge/"))     p.agentName = "Edge";
    else if (contains("Firefox/"))                 p.agentName = "Firefox";
    else if (contains("Chrome/"))                  p.agentName = "Chrome";
    else if (contains("Safari/"))                  p.agentName = "Safari";

    return p;
}

std::string makeDeviceName(const ParsedAgent& pa,
                           const std::string& deviceId) {
    // V1.1.1 : on ajoute un suffixe court (4 hex) tiré du device_id pour
    // distinguer visuellement plusieurs navigateurs sur le même OS/browser.
    // Ex: deux Safari sur iOS → "iOS (Safari) · a3f2" et "iOS (Safari) · 7e91".
    std::string suffix;
    for (char c : deviceId) {
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F')) {
            suffix.push_back(c);
            if (suffix.size() >= 4) break;
        }
    }
    return pa.platform + " (" + pa.agentName + ")"
         + (suffix.empty() ? "" : (" · " + suffix));
}

std::string makeToken() {
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::string token;
    token.reserve(32);
    for (int i = 0; i < 16; ++i) {
        const auto b = static_cast<std::uint8_t>(rng() & 0xFF);
        constexpr char kHex[] = "0123456789abcdef";
        token.push_back(kHex[(b >> 4) & 0xF]);
        token.push_back(kHex[b & 0xF]);
    }
    return token;
}

} // namespace

std::optional<std::string> WebSessionStore::authenticate(
    const std::string& providedPin,
    const std::string& expectedPin,
    const std::string& deviceId,
    const std::string& userAgent) {

    if (expectedPin.empty() || providedPin != expectedPin) return std::nullopt;
    if (deviceId.empty()) return std::nullopt;

    const auto token = makeToken();
    const auto pa = parseUserAgent(userAgent);
    const auto dn = DisplayName::fromDeviceIdAndUA(deviceId, userAgent);

    WebSession sess;
    sess.token     = token;
    sess.deviceId  = deviceId;
    sess.userAgent = userAgent;
    sess.lastSeen  = std::chrono::steady_clock::now();

    sess.device.id           = deviceId;           // V1.1 : id stable = deviceId
    sess.device.name         = makeDeviceName(pa, deviceId);
    sess.device.platform     = pa.platform;
    sess.device.kind         = domain::PeerKind::Web;
    sess.device.sessionToken = token;              // éphémère
    sess.device.lastSeen     = sess.lastSeen;

    // V1.2 — Sprint Web P2P : auto-générés, stables (hash deviceId).
    sess.displayName    = dn.name;
    sess.emoji          = dn.emoji;
    sess.platformLabel  = dn.platformLabel;

    {
        std::lock_guard<std::mutex> lock(mu_);
        // Si ce device avait déjà une session active, l'invalider.
        const auto it = deviceToToken_.find(deviceId);
        if (it != deviceToToken_.end()) {
            sessions_.erase(it->second);
        }
        deviceToToken_[deviceId] = token;
        sessions_.emplace(token, std::move(sess));
    }
    return token;
}

std::optional<WebSession> WebSessionStore::validate(
    const std::string& token) const {
    if (token.empty()) return std::nullopt;
    std::lock_guard<std::mutex> lock(mu_);
    const auto it = sessions_.find(token);
    if (it == sessions_.end()) return std::nullopt;

    const auto now = std::chrono::steady_clock::now();
    if (now - it->second.lastSeen > core::kWebSessionTtl) return std::nullopt;
    return it->second;
}

void WebSessionStore::touch(const std::string& token) {
    std::lock_guard<std::mutex> lock(mu_);
    const auto it = sessions_.find(token);
    if (it == sessions_.end()) return;
    it->second.lastSeen        = std::chrono::steady_clock::now();
    it->second.device.lastSeen = it->second.lastSeen;
}

std::vector<WebSessionStore::EvictedSession>
WebSessionStore::evictExpired() {
    const auto now = std::chrono::steady_clock::now();
    std::vector<EvictedSession> expired;
    std::lock_guard<std::mutex> lock(mu_);
    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        if (now - it->second.lastSeen > core::kWebSessionTtl) {
            expired.push_back({it->first, it->second.deviceId});
            // Nettoyer la table inverse si elle pointe toujours vers ce token.
            const auto di = deviceToToken_.find(it->second.deviceId);
            if (di != deviceToToken_.end() && di->second == it->first) {
                deviceToToken_.erase(di);
            }
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
    return expired;
}

std::vector<WebSession> WebSessionStore::snapshot() const {
    std::vector<WebSession> out;
    std::lock_guard<std::mutex> lock(mu_);
    out.reserve(sessions_.size());
    for (const auto& [_, s] : sessions_) out.push_back(s);
    return out;
}

void WebSessionStore::removeByToken(const std::string& token) {
    std::lock_guard<std::mutex> lock(mu_);
    const auto it = sessions_.find(token);
    if (it == sessions_.end()) return;
    const auto di = deviceToToken_.find(it->second.deviceId);
    if (di != deviceToToken_.end() && di->second == token) {
        deviceToToken_.erase(di);
    }
    sessions_.erase(it);
}

void WebSessionStore::removeByDeviceId(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mu_);
    const auto it = deviceToToken_.find(deviceId);
    if (it == deviceToToken_.end()) return;
    sessions_.erase(it->second);
    deviceToToken_.erase(it);
}

std::vector<WebSessionStore::PeerInfo>
WebSessionStore::snapshotPeersFor(const std::string& excludeToken) const {
    std::vector<PeerInfo> out;
    std::lock_guard<std::mutex> lock(mu_);
    out.reserve(sessions_.size());
    for (const auto& [tok, s] : sessions_) {
        if (tok == excludeToken) continue;
        out.push_back({s.deviceId, s.displayName,
                       s.emoji,    s.platformLabel});
    }
    return out;
}

std::optional<std::string>
WebSessionStore::findTokenByDeviceId(const std::string& deviceId) const {
    if (deviceId.empty()) return std::nullopt;
    std::lock_guard<std::mutex> lock(mu_);
    const auto it = deviceToToken_.find(deviceId);
    if (it == deviceToToken_.end()) return std::nullopt;
    return it->second;
}

} // namespace ltr::web
