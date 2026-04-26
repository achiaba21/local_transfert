#include "ltr/web/web_upload_announce.hpp"

#include <random>

namespace ltr::web {

namespace {

std::string makeUploadId() {
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

std::string WebUploadAnnounceStore::create(
    const std::string& sessionToken,
    const std::string& senderName,
    std::vector<AnnounceFile> files) {
    auto a = std::make_shared<WebUploadAnnounce>();
    a->uploadId     = makeUploadId();
    a->sessionToken = sessionToken;
    a->senderName   = senderName;
    a->files        = std::move(files);
    for (const auto& f : a->files) a->totalBytes += f.size;

    const auto id = a->uploadId;
    std::lock_guard<std::mutex> lock(mu_);
    map_.emplace(id, std::move(a));
    return id;
}

AnnounceSnapshot WebUploadAnnounceStore::waitForDecision(
    const std::string& uploadId,
    std::chrono::milliseconds timeout) {
    std::shared_ptr<WebUploadAnnounce> a;
    {
        std::lock_guard<std::mutex> lock(mu_);
        const auto it = map_.find(uploadId);
        if (it == map_.end()) {
            AnnounceSnapshot stub;
            stub.decision = AnnounceDecision::TimedOut;
            return stub;
        }
        a = it->second;
    }

    std::unique_lock<std::mutex> lock(a->mu);
    a->cv.wait_for(lock, timeout, [&]{
        return a->decision != AnnounceDecision::Pending;
    });
    if (a->decision == AnnounceDecision::Pending) {
        a->decision = AnnounceDecision::TimedOut;
    }
    return a->snapshot();
}

bool WebUploadAnnounceStore::resolveAccept(
    const std::string& uploadId,
    const std::filesystem::path& targetDir) {
    std::shared_ptr<WebUploadAnnounce> a;
    {
        std::lock_guard<std::mutex> lock(mu_);
        const auto it = map_.find(uploadId);
        if (it == map_.end()) return false;
        a = it->second;
    }
    {
        std::lock_guard<std::mutex> lock(a->mu);
        if (a->decision != AnnounceDecision::Pending) return false;
        a->decision  = AnnounceDecision::Accepted;
        a->targetDir = targetDir;
    }
    a->cv.notify_all();
    return true;
}

bool WebUploadAnnounceStore::resolveRefuse(const std::string& uploadId) {
    std::shared_ptr<WebUploadAnnounce> a;
    {
        std::lock_guard<std::mutex> lock(mu_);
        const auto it = map_.find(uploadId);
        if (it == map_.end()) return false;
        a = it->second;
    }
    {
        std::lock_guard<std::mutex> lock(a->mu);
        if (a->decision != AnnounceDecision::Pending) return false;
        a->decision = AnnounceDecision::Refused;
    }
    a->cv.notify_all();
    return true;
}

std::optional<AnnounceSnapshot> WebUploadAnnounceStore::peek(
    const std::string& uploadId) const {
    std::shared_ptr<WebUploadAnnounce> a;
    {
        std::lock_guard<std::mutex> lock(mu_);
        const auto it = map_.find(uploadId);
        if (it == map_.end()) return std::nullopt;
        a = it->second;
    }
    std::lock_guard<std::mutex> l(a->mu);
    return a->snapshot();
}

void WebUploadAnnounceStore::remove(const std::string& uploadId) {
    std::lock_guard<std::mutex> lock(mu_);
    map_.erase(uploadId);
}

} // namespace ltr::web
