#pragma once

namespace ltr::ui {

// Animation float-based : interpole `from` → `to` sur `duration` secondes
// avec une fonction d'easing. Sprint UI Layout System.
//
// Usage :
//   Animation fade_;
//   fade_.start(0.f, 1.f, 0.2f);   // au moment de l'apparition
//   // chaque frame :
//   fade_.update(dt.asSeconds());
//   const float alpha = fade_.value();
//   if (fade_.finished()) {  // remove de la collection si nécessaire }
class Animation {
public:
    enum class Easing { Linear, EaseOut, EaseInOut };

    void start(float from, float to, float durationSec,
               Easing easing = Easing::EaseOut);
    void update(float dt);

    bool  active()   const { return duration_ > 0.f && elapsed_ < duration_; }
    bool  finished() const { return duration_ > 0.f && elapsed_ >= duration_; }
    float value()    const;

private:
    float  from_{0.f};
    float  to_{0.f};
    float  duration_{0.f};   // 0 = inactive
    float  elapsed_{0.f};
    Easing easing_{Easing::EaseOut};
};

} // namespace ltr::ui
