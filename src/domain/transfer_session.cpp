#include "ltr/domain/transfer_session.hpp"

#include <algorithm>

namespace ltr::domain {

namespace {
constexpr auto kWindowDuration = std::chrono::seconds(3);
}

TransferSession::TransferSession(std::string sessionId,
                                 Direction direction,
                                 Device peer,
                                 std::uint64_t totalBytes)
    : sessionId_(std::move(sessionId)),
      direction_(direction),
      peer_(std::move(peer)),
      totalBytes_(totalBytes) {
    window_.push_back({std::chrono::steady_clock::now(), 0});
}

TransferStatus TransferSession::status() const {
    std::lock_guard<std::mutex> lock(mu_);
    return status_;
}

void TransferSession::setStatus(TransferStatus s) {
    std::lock_guard<std::mutex> lock(mu_);
    status_ = s;
}

std::uint64_t TransferSession::bytesTransferred() const {
    std::lock_guard<std::mutex> lock(mu_);
    return bytesTransferred_;
}

void TransferSession::addBytes(std::uint64_t n) {
    std::lock_guard<std::mutex> lock(mu_);
    bytesTransferred_ += n;

    const auto now = std::chrono::steady_clock::now();
    window_.push_back({now, bytesTransferred_});
    while (window_.size() > 1 && (now - window_.front().t) > kWindowDuration) {
        window_.pop_front();
    }
}

double TransferSession::speedBytesPerSec() const {
    std::lock_guard<std::mutex> lock(mu_);
    if (window_.size() < 2) return 0.0;
    const auto& first = window_.front();
    const auto& last = window_.back();
    const double dt =
        std::chrono::duration<double>(last.t - first.t).count();
    if (dt <= 0.0) return 0.0;
    return static_cast<double>(last.totalSoFar - first.totalSoFar) / dt;
}

std::chrono::seconds TransferSession::eta() const {
    const double spd = speedBytesPerSec();
    std::lock_guard<std::mutex> lock(mu_);
    if (spd <= 1.0 || bytesTransferred_ >= totalBytes_) {
        return std::chrono::seconds(0);
    }
    const auto remaining = totalBytes_ - bytesTransferred_;
    const double seconds = static_cast<double>(remaining) / spd;
    return std::chrono::seconds(static_cast<long long>(seconds));
}

double TransferSession::progress() const {
    std::lock_guard<std::mutex> lock(mu_);
    if (totalBytes_ == 0) return 1.0;
    return std::min(1.0,
        static_cast<double>(bytesTransferred_) /
        static_cast<double>(totalBytes_));
}

} // namespace ltr::domain
