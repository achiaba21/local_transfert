// Sprint UI Layout System — tests du DSL HBox/VBox.
//
// Vérifie la répartition fixed/expanded, padding, spacing.

#include "ltr/ui/layout.hpp"

#include <cassert>
#include <iostream>

namespace {

bool nearly(float a, float b, float eps = 0.01f) {
    return std::abs(a - b) < eps;
}

} // namespace

int main() {
    using namespace ltr::ui;

    // 1) HBox simple sans padding/spacing : 3 enfants, 100/200/100,
    //    parent 600 → tout fixed, on attend les rects collés.
    {
        HBox box;
        box.fixed(100.f, nullptr)
           .fixed(200.f, nullptr)
           .fixed(100.f, nullptr);
        const auto rs = box.computeBounds({0.f, 0.f, 600.f, 50.f});
        assert(rs.size() == 3);
        assert(nearly(rs[0].left,   0.f));
        assert(nearly(rs[0].width, 100.f));
        assert(nearly(rs[1].left, 100.f));
        assert(nearly(rs[1].width, 200.f));
        assert(nearly(rs[2].left, 300.f));
        assert(nearly(rs[2].width, 100.f));
        std::cout << "  [ok] HBox fixed sans gap\n";
    }

    // 2) HBox avec spacing 10 + padding 8 : 3 fixed 50.
    {
        HBox box;
        box.padding(8.f).spacing(10.f);
        box.fixed(50.f, nullptr).fixed(50.f, nullptr).fixed(50.f, nullptr);
        const auto rs = box.computeBounds({0.f, 0.f, 200.f, 30.f});
        assert(rs.size() == 3);
        assert(nearly(rs[0].left,  8.f));
        assert(nearly(rs[1].left,  8.f + 50.f + 10.f));   // 68
        assert(nearly(rs[2].left,  8.f + 50.f + 10.f + 50.f + 10.f)); // 128
        // Hauteur intérieure = 30 - 16 = 14
        assert(nearly(rs[0].top,    8.f));
        assert(nearly(rs[0].height, 14.f));
        std::cout << "  [ok] HBox padding+spacing\n";
    }

    // 3) HBox avec expanded : 1 fixed 100 + 1 expanded(2) + 1 expanded(1)
    //    parent 700, padding 0, spacing 0
    //    available = 700 - 100 = 600 ; 3 weights → expanded(2)=400, expanded(1)=200
    {
        HBox box;
        box.fixed(100.f, nullptr)
           .expanded(2, nullptr)
           .expanded(1, nullptr);
        const auto rs = box.computeBounds({0.f, 0.f, 700.f, 50.f});
        assert(nearly(rs[0].width, 100.f));
        assert(nearly(rs[1].width, 400.f));
        assert(nearly(rs[2].width, 200.f));
        assert(nearly(rs[1].left,  100.f));
        assert(nearly(rs[2].left,  500.f));
        std::cout << "  [ok] HBox expanded weights\n";
    }

    // 4) VBox vertical
    {
        VBox box;
        box.fixed(40.f, nullptr).expanded(1, nullptr).fixed(30.f, nullptr);
        const auto rs = box.computeBounds({0.f, 0.f, 200.f, 200.f});
        assert(nearly(rs[0].height, 40.f));
        assert(nearly(rs[1].height, 130.f));   // 200 - 40 - 30
        assert(nearly(rs[2].height, 30.f));
        assert(nearly(rs[0].top,   0.f));
        assert(nearly(rs[1].top,   40.f));
        assert(nearly(rs[2].top, 170.f));
        // VBox : tous les enfants prennent toute la largeur
        assert(nearly(rs[0].width, 200.f));
        assert(nearly(rs[1].width, 200.f));
        std::cout << "  [ok] VBox\n";
    }

    // 5) Spacer (taille fixe sans dessin)
    {
        HBox box;
        box.fixed(50.f, nullptr).spacer(20.f).fixed(30.f, nullptr);
        const auto rs = box.computeBounds({0.f, 0.f, 100.f, 10.f});
        assert(nearly(rs[1].left,  50.f));
        assert(nearly(rs[1].width, 20.f));
        assert(nearly(rs[2].left,  70.f));
        std::cout << "  [ok] HBox spacer\n";
    }

    // 6) Edge case : parent trop petit pour les fixed
    {
        HBox box;
        box.fixed(100.f, nullptr).fixed(100.f, nullptr).expanded(1, nullptr);
        const auto rs = box.computeBounds({0.f, 0.f, 150.f, 30.f});
        // expanded.width devrait être max(0, ...) = 0
        assert(rs.size() == 3);
        assert(rs[2].width >= 0.f);
        std::cout << "  [ok] HBox parent too small (graceful)\n";
    }

    std::cout << "test_layout_box OK\n";
    return 0;
}
