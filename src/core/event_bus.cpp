#include "ltr/core/event_bus.hpp"

namespace ltr::core {

void EventBus::post(Event e) {
    std::lock_guard<std::mutex> lock(mu_);
    q_.push(std::move(e));
}

std::vector<Event> EventBus::drain() {
    std::vector<Event> out;
    std::lock_guard<std::mutex> lock(mu_);
    out.reserve(q_.size());
    while (!q_.empty()) {
        out.push_back(std::move(q_.front()));
        q_.pop();
    }
    return out;
}

} // namespace ltr::core
