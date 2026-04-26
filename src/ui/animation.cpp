#include "ltr/ui/animation.hpp"

#include <algorithm>
#include <cmath>

namespace ltr::ui {

void Animation::start(float from, float to, float durationSec,
                       Easing easing) {
    from_     = from;
    to_       = to;
    duration_ = std::max(0.f, durationSec);
    elapsed_  = 0.f;
    easing_   = easing;
}

void Animation::update(float dt) {
    if (duration_ <= 0.f) return;
    elapsed_ = std::min(duration_, elapsed_ + std::max(0.f, dt));
}

float Animation::value() const {
    if (duration_ <= 0.f) return to_;
    const float t = std::clamp(elapsed_ / duration_, 0.f, 1.f);
    float eased = t;
    switch (easing_) {
        case Easing::Linear:
            eased = t;
            break;
        case Easing::EaseOut: {
            // 1 - (1-t)^3
            const float u = 1.f - t;
            eased = 1.f - u * u * u;
            break;
        }
        case Easing::EaseInOut: {
            eased = (t < 0.5f)
                ? 4.f * t * t * t
                : 1.f - std::pow(-2.f * t + 2.f, 3.f) / 2.f;
            break;
        }
    }
    return from_ + (to_ - from_) * eased;
}

} // namespace ltr::ui
