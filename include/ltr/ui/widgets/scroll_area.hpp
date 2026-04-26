#pragma once

#include <SFML/Graphics/Rect.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Window/Event.hpp>

namespace ltr::ui {

// Sprint UI Layout System : widget scroll réutilisable.
//
// Usage typique :
//   ScrollArea sidebar_;
//   sidebar_.setBounds(sidebarRect);
//   sidebar_.setDirection(ScrollArea::Direction::Vertical);
//   sidebar_.setContentSize(0.f, peerCount * itemH);
//   sidebar_.handleEvent(e);  // dans handleEvent
//   // dans draw :
//   {
//       ClipScope clip(target, sidebar_.viewport());
//       sidebar_.forEachVisible(peerCount,
//           [](size_t i){ return sf::FloatRect{0, i*itemH, w, itemH}; },
//           [&](size_t i, const sf::FloatRect& screen){
//               drawPeer(target, peers[i], screen);
//           });
//   }
//   sidebar_.draw(target);  // dessine la scrollbar
class ScrollArea {
public:
    enum class Direction { Vertical, Horizontal, Both };

    ScrollArea() = default;

    ScrollArea& setBounds(const sf::FloatRect& r);
    ScrollArea& setDirection(Direction d);
    ScrollArea& setContentSize(float w, float h);
    ScrollArea& showScrollbar(bool b);

    bool handleEvent(const sf::Event& e);  // return true si consommé
    void draw(sf::RenderTarget& t) const;   // dessine la scrollbar

    const sf::FloatRect& viewport() const { return bounds_; }
    float scrollX() const { return scrollX_; }
    float scrollY() const { return scrollY_; }
    float contentW() const { return contentW_; }
    float contentH() const { return contentH_; }

    // Itère sur les items visibles. itemPos(i) → rect en coordonnées
    // CONTENU (relatif au viewport, sans scroll). fn(i, screenRect) est
    // appelé avec le rect translaté en coordonnées ÉCRAN après scroll.
    template <typename ItemPosFn, typename DrawFn>
    void forEachVisible(std::size_t count,
                         ItemPosFn itemPos, DrawFn fn) const {
        for (std::size_t i = 0; i < count; ++i) {
            const auto local = itemPos(i);
            sf::FloatRect screen{
                bounds_.left + local.left - scrollX_,
                bounds_.top  + local.top  - scrollY_,
                local.width, local.height
            };
            // Skip si entièrement hors viewport
            if (screen.left + screen.width  < bounds_.left)            continue;
            if (screen.top  + screen.height < bounds_.top)             continue;
            if (screen.left > bounds_.left + bounds_.width)            break;
            if (screen.top  > bounds_.top  + bounds_.height)           break;
            fn(i, screen);
        }
    }

    // Reset le scroll à (0, 0).
    void reset() { scrollX_ = scrollY_ = 0.f; }

private:
    void clamp();

    sf::FloatRect bounds_{};
    Direction     dir_{Direction::Vertical};
    float         contentW_{0.f}, contentH_{0.f};
    float         scrollX_{0.f}, scrollY_{0.f};
    bool          showScrollbar_{true};
};

} // namespace ltr::ui
