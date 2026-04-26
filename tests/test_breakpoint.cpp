// Sprint UI Layout System — tests des breakpoints responsive.

#include "ltr/ui/breakpoint.hpp"

#include <cassert>
#include <iostream>

int main() {
    using namespace ltr::ui;

    // Compact < 800
    assert(detectBreakpoint(0)    == Breakpoint::Compact);
    assert(detectBreakpoint(400)  == Breakpoint::Compact);
    assert(detectBreakpoint(799)  == Breakpoint::Compact);
    // Regular 800..1299
    assert(detectBreakpoint(800)  == Breakpoint::Regular);
    assert(detectBreakpoint(1100) == Breakpoint::Regular);
    assert(detectBreakpoint(1299) == Breakpoint::Regular);
    // Large >= 1300
    assert(detectBreakpoint(1300) == Breakpoint::Large);
    assert(detectBreakpoint(2560) == Breakpoint::Large);

    // Metrics
    const auto compact = metricsFor(Breakpoint::Compact);
    assert(compact.forceSharePanelCollapsed == true);

    const auto regular = metricsFor(Breakpoint::Regular);
    assert(regular.forceSharePanelCollapsed == false);

    const auto large = metricsFor(Breakpoint::Large);
    assert(large.sidebarW > regular.sidebarW);
    assert(large.sharePanelExpandedW > regular.sharePanelExpandedW);

    std::cout << "test_breakpoint OK\n";
    return 0;
}
