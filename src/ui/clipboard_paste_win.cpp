// V1.4 — Sprint Clipboard Paste (Windows / Win32 OpenClipboard).
//
// Lit successivement CF_HDROP (fichiers), CF_DIB (image — convertie en
// PNG via miniz), CF_UNICODETEXT (texte). OpenClipboard est sérialisé
// par le système → retry 3× × 10 ms si une autre app le tient.

#include "ltr/ui/clipboard_paste.hpp"

#include <windows.h>
#include <shellapi.h>
#include <thread>
#include <chrono>

#include "ltr/core/logger.hpp"
#include "ltr/core/png_encoder.hpp"

namespace ltr::ui {

namespace {

bool openClipboardWithRetry() {
    for (int i = 0; i < 3; ++i) {
        if (OpenClipboard(nullptr)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

// Convertit UTF-16 (WCHAR*) en UTF-8.
std::string utf16ToUtf8(const wchar_t* w) {
    if (!w) return {};
    const int needed = WideCharToMultiByte(CP_UTF8, 0, w, -1,
                                            nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string out(static_cast<std::size_t>(needed - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), needed,
                        nullptr, nullptr);
    return out;
}

// V1.5 — l'encodeur PNG est extrait dans core::encodePng (réutilisable).
// On ne garde ici que la conversion DIB → RGBA top-down.

// Convertit un BITMAPINFO + bits DIB en PNG. Gère 24 bits BGR et
// 32 bits BGRA. Pour les autres profondeurs, retourne vide.
std::vector<std::uint8_t> dibToPng(const BITMAPINFO* bmi, const void* bits) {
    if (!bmi || !bits) return {};
    const auto& h = bmi->bmiHeader;
    if (h.biPlanes != 1) return {};
    if (h.biBitCount != 24 && h.biBitCount != 32) return {};

    const std::int32_t width  = h.biWidth;
    const std::int32_t height = (h.biHeight < 0) ? -h.biHeight : h.biHeight;
    const bool topDown = (h.biHeight < 0);
    if (width <= 0 || height <= 0) return {};

    const std::size_t bpp = h.biBitCount / 8;
    const std::size_t rowSize  = ((width * h.biBitCount + 31) / 32) * 4;
    const auto* src = static_cast<const std::uint8_t*>(bits);

    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * height * 4);
    for (std::int32_t y = 0; y < height; ++y) {
        const std::int32_t srcY = topDown ? y : (height - 1 - y);
        const std::uint8_t* srcRow = src + static_cast<std::size_t>(srcY) * rowSize;
        std::uint8_t* dstRow = rgba.data()
                                + static_cast<std::size_t>(y) * width * 4;
        for (std::int32_t x = 0; x < width; ++x) {
            const std::uint8_t* p = srcRow + x * bpp;
            // DIB est BGR(A) → on convertit en RGBA.
            dstRow[x * 4 + 0] = p[2];                       // R
            dstRow[x * 4 + 1] = p[1];                       // G
            dstRow[x * 4 + 2] = p[0];                       // B
            dstRow[x * 4 + 3] = (bpp == 4) ? p[3] : 255;    // A
        }
    }
    return ltr::core::encodePng(static_cast<std::uint32_t>(width),
                                 static_cast<std::uint32_t>(height), rgba);
}

} // namespace

ClipboardPaste readClipboard() {
    ClipboardPaste out;

    if (!openClipboardWithRetry()) {
        core::log_warn("[clipboard/win] OpenClipboard failed");
        return out;
    }

    // 1. Files (CF_HDROP).
    if (HANDLE h = GetClipboardData(CF_HDROP)) {
        auto* drop = static_cast<HDROP>(GlobalLock(h));
        if (drop) {
            const UINT n = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            for (UINT i = 0; i < n; ++i) {
                const UINT need = DragQueryFileW(drop, i, nullptr, 0);
                std::wstring buf(need, L'\0');
                DragQueryFileW(drop, i, buf.data(), need + 1);
                out.files.emplace_back(buf);
            }
            GlobalUnlock(h);
        }
        if (!out.files.empty()) {
            out.kind = ClipboardPaste::Kind::Files;
            CloseClipboard();
            core::log_info("[clipboard/win] Kind=Files n="
                           + std::to_string(out.files.size()));
            return out;
        }
    }

    // 2. Image (CF_DIB → PNG).
    if (HANDLE h = GetClipboardData(CF_DIB)) {
        auto* mem = static_cast<std::uint8_t*>(GlobalLock(h));
        if (mem) {
            const auto* bmi = reinterpret_cast<const BITMAPINFO*>(mem);
            const std::size_t bmiSize = bmi->bmiHeader.biSize;
            const std::size_t paletteSize = (bmi->bmiHeader.biBitCount <= 8)
                ? (1u << bmi->bmiHeader.biBitCount) * sizeof(RGBQUAD) : 0;
            const void* bits = mem + bmiSize + paletteSize;
            auto bytes = dibToPng(bmi, bits);
            GlobalUnlock(h);
            if (!bytes.empty()) {
                out.kind = ClipboardPaste::Kind::Image;
                out.imageBytes = std::move(bytes);
                out.imageExt = "png";
                CloseClipboard();
                core::log_info("[clipboard/win] Kind=Image PNG bytes="
                               + std::to_string(out.imageBytes.size()));
                return out;
            }
        }
    }

    // 3. Text (CF_UNICODETEXT).
    if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) {
        auto* w = static_cast<wchar_t*>(GlobalLock(h));
        if (w && *w) {
            out.kind = ClipboardPaste::Kind::Text;
            out.text = utf16ToUtf8(w);
        }
        if (w) GlobalUnlock(h);
        if (!out.text.empty()) {
            CloseClipboard();
            core::log_info("[clipboard/win] Kind=Text len="
                           + std::to_string(out.text.size()));
            return out;
        }
    }

    CloseClipboard();
    core::log_info("[clipboard/win] Kind=None");
    return out;
}

} // namespace ltr::ui
