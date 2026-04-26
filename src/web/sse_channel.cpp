#include "ltr/web/sse_channel.hpp"

namespace ltr::web {

void SseChannel::push(std::string msg) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (closed_) return;
        queue_.push_back(std::move(msg));
    }
    cv_.notify_one();
}

bool SseChannel::waitAndPop(std::string& out,
                            std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait_for(lock, timeout, [this]{
        return closed_ || !queue_.empty();
    });
    if (queue_.empty()) return false;
    out = std::move(queue_.front());
    queue_.pop_front();
    return true;
}

void SseChannel::close() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        closed_ = true;
    }
    cv_.notify_all();
}

bool SseChannel::isClosed() const {
    std::lock_guard<std::mutex> lock(mu_);
    return closed_;
}

} // namespace ltr::web
