// Sprint UX-3 : support drag & drop depuis Explorer → fenêtre SFML.
// Approche simple : DragAcceptFiles + WM_DROPFILES via SetWindowSubclass.
// Limitation V1 : pas d'events drag-over (seul le drop final fire).
// Upgrade V2 possible via IDropTarget COM si besoin de feedback visuel
// pendant le drag.

#include "ltr/ui/drag_drop.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>

#include "ltr/core/logger.hpp"

namespace ltr::ui {

namespace {

// Convertit wide string (UTF-16) → UTF-8.
std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, w.data(),
                                            static_cast<int>(w.size()),
                                            nullptr, 0, nullptr, nullptr);
    std::string out(needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                        out.data(), needed, nullptr, nullptr);
    return out;
}

constexpr UINT_PTR kSubclassId = 0x4c54'5244; // 'LTRD'

LRESULT CALLBACK dropSubclassProc(HWND hwnd, UINT msg, WPARAM wp,
                                    LPARAM lp, UINT_PTR id,
                                    DWORD_PTR ref) {
    if (msg == WM_DROPFILES) {
        auto* cbs = reinterpret_cast<DragDropHandler::Callbacks*>(ref);
        if (cbs && cbs->onDrop) {
            HDROP hdrop = reinterpret_cast<HDROP>(wp);
            const UINT count = DragQueryFileW(hdrop, 0xFFFFFFFF,
                                               nullptr, 0);
            std::vector<std::filesystem::path> paths;
            paths.reserve(count);
            for (UINT i = 0; i < count; ++i) {
                const UINT len = DragQueryFileW(hdrop, i, nullptr, 0);
                std::wstring buf(len + 1, L'\0');
                DragQueryFileW(hdrop, i, buf.data(), len + 1);
                buf.resize(len);
                paths.emplace_back(wideToUtf8(buf));
            }
            DragFinish(hdrop);
            cbs->onDrop(std::move(paths));
        }
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp, id, ref);
}

} // namespace

struct DragDropHandler::Impl {
    HWND hwnd{nullptr};
    std::unique_ptr<Callbacks> cbs;
};

DragDropHandler::DragDropHandler() : impl_(std::make_unique<Impl>()) {}
DragDropHandler::~DragDropHandler() { detach(); }

bool DragDropHandler::attach(sf::WindowHandle handle, Callbacks cb) {
    HWND hwnd = reinterpret_cast<HWND>(handle);
    if (!hwnd) {
        core::log_error("[drag-drop/win] HWND null");
        return false;
    }
    impl_->hwnd = hwnd;
    impl_->cbs = std::make_unique<Callbacks>(std::move(cb));

    DragAcceptFiles(hwnd, TRUE);
    SetWindowSubclass(hwnd, dropSubclassProc, kSubclassId,
                      reinterpret_cast<DWORD_PTR>(impl_->cbs.get()));

    core::log_info("[drag-drop/win] attached");
    return true;
}

void DragDropHandler::detach() {
    if (!impl_ || !impl_->hwnd) return;
    DragAcceptFiles(impl_->hwnd, FALSE);
    RemoveWindowSubclass(impl_->hwnd, dropSubclassProc, kSubclassId);
    impl_->hwnd = nullptr;
    impl_->cbs.reset();
}

} // namespace ltr::ui
