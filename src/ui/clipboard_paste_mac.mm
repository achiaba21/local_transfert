// V1.4 — Sprint Clipboard Paste (macOS / NSPasteboard).
// Objective-C++ avec ARC (compilé via -fobjc-arc en CMake).

#include "ltr/ui/clipboard_paste.hpp"

#import <Cocoa/Cocoa.h>

#include "ltr/core/logger.hpp"

namespace ltr::ui {

namespace {

// Convertit un NSData (PNG/TIFF/etc.) en vector<uint8_t>.
std::vector<std::uint8_t> nsDataToBytes(NSData* data) {
    std::vector<std::uint8_t> out;
    if (!data) return out;
    const auto* bytes = static_cast<const std::uint8_t*>(data.bytes);
    out.assign(bytes, bytes + data.length);
    return out;
}

// Convertit un NSImage / NSData TIFF en PNG via NSBitmapImageRep.
std::vector<std::uint8_t> tiffToPng(NSData* tiffData) {
    if (!tiffData) return {};
    NSBitmapImageRep* rep = [NSBitmapImageRep imageRepWithData:tiffData];
    if (!rep) return {};
    NSData* png = [rep representationUsingType:NSBitmapImageFileTypePNG
                                    properties:@{}];
    return nsDataToBytes(png);
}

} // namespace

ClipboardPaste readClipboard() {
    ClipboardPaste out;

    NSPasteboard* pb = [NSPasteboard generalPasteboard];
    if (!pb) {
        core::log_warn("[clipboard/mac] generalPasteboard nil");
        return out;
    }

    // 1. Files (priorité la plus haute).
    NSArray<NSURL*>* urls = [pb readObjectsForClasses:@[[NSURL class]]
                                              options:nil];
    if (urls.count > 0) {
        for (NSURL* u in urls) {
            if (!u.isFileURL) continue;
            const char* p = u.path.fileSystemRepresentation;
            if (p && *p) out.files.emplace_back(p);
        }
        if (!out.files.empty()) {
            out.kind = ClipboardPaste::Kind::Files;
            core::log_info("[clipboard/mac] Kind=Files n="
                           + std::to_string(out.files.size()));
            return out;
        }
    }

    // 2. Image (PNG préférée, fallback TIFF→PNG).
    NSData* png = [pb dataForType:NSPasteboardTypePNG];
    if (png && png.length > 0) {
        out.kind = ClipboardPaste::Kind::Image;
        out.imageBytes = nsDataToBytes(png);
        out.imageExt = "png";
        core::log_info("[clipboard/mac] Kind=Image PNG bytes="
                       + std::to_string(out.imageBytes.size()));
        return out;
    }
    NSData* tiff = [pb dataForType:NSPasteboardTypeTIFF];
    if (tiff && tiff.length > 0) {
        auto bytes = tiffToPng(tiff);
        if (!bytes.empty()) {
            out.kind = ClipboardPaste::Kind::Image;
            out.imageBytes = std::move(bytes);
            out.imageExt = "png";
            core::log_info("[clipboard/mac] Kind=Image TIFF→PNG bytes="
                           + std::to_string(out.imageBytes.size()));
            return out;
        }
    }

    // 3. Text.
    NSString* str = [pb stringForType:NSPasteboardTypeString];
    if (str && str.length > 0) {
        out.kind = ClipboardPaste::Kind::Text;
        out.text = std::string(str.UTF8String ? str.UTF8String : "");
        core::log_info("[clipboard/mac] Kind=Text len="
                       + std::to_string(out.text.size()));
        return out;
    }

    core::log_info("[clipboard/mac] Kind=None");
    return out;
}

} // namespace ltr::ui
