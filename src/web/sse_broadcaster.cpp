#include "ltr/web/sse_broadcaster.hpp"

namespace ltr::web {

std::shared_ptr<SseChannel> SseBroadcaster::attach(
    const std::string& sessionToken) {
    std::lock_guard<std::mutex> lock(mu_);
    auto& existing = channels_[sessionToken];
    if (!existing) existing = std::make_shared<SseChannel>();
    return existing;
}

void SseBroadcaster::detach(const std::string& sessionToken) {
    std::shared_ptr<SseChannel> toClose;
    {
        std::lock_guard<std::mutex> lock(mu_);
        const auto it = channels_.find(sessionToken);
        if (it != channels_.end()) {
            toClose = it->second;
            channels_.erase(it);
        }
    }
    if (toClose) toClose->close();
}

void SseBroadcaster::send(const std::string& sessionToken,
                          const std::string& message) {
    std::shared_ptr<SseChannel> ch;
    {
        std::lock_guard<std::mutex> lock(mu_);
        const auto it = channels_.find(sessionToken);
        if (it != channels_.end()) ch = it->second;
    }
    if (ch) ch->push(message);
}

void SseBroadcaster::closeAll() {
    std::map<std::string, std::shared_ptr<SseChannel>> snapshot;
    {
        std::lock_guard<std::mutex> lock(mu_);
        snapshot = std::move(channels_);
        channels_.clear();
    }
    for (auto& [_, ch] : snapshot) if (ch) ch->close();
}

} // namespace ltr::web
