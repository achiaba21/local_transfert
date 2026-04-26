#pragma once

namespace ltr::ui {

// Sprint UI Layout System : 3 breakpoints responsive selon la largeur
// fenêtre.
enum class Breakpoint {
    Compact,   // < 800 px : sidebar masquée derrière bouton ☰, sharePanel
               // forcé collapsed
    Regular,   // 800-1300 px : layout standard
    Large,     // > 1300 px : sidebar 360 px, sharePanel 320 px
};

constexpr unsigned kCompactMaxPx = 800;
constexpr unsigned kRegularMaxPx = 1300;

inline Breakpoint detectBreakpoint(unsigned widthPx) {
    if (widthPx < kCompactMaxPx) return Breakpoint::Compact;
    if (widthPx < kRegularMaxPx) return Breakpoint::Regular;
    return Breakpoint::Large;
}

// Largeurs sidebar / sharePanel selon breakpoint.
struct LayoutMetrics {
    float sidebarW;
    float sharePanelExpandedW;
    bool  forceSharePanelCollapsed;
};

inline LayoutMetrics metricsFor(Breakpoint bp) {
    switch (bp) {
        case Breakpoint::Compact:
            return {300.f, 240.f, true};   // sharePanel forcé collapsed
        case Breakpoint::Regular:
            return {300.f, 240.f, false};
        case Breakpoint::Large:
            return {360.f, 320.f, false};
    }
    return {300.f, 240.f, false};
}

} // namespace ltr::ui
