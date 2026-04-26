#include "ltr/web/self_binary.hpp"

#include <cstring>
#include <fstream>
#include <vector>

#include <miniz.h>

#include "ltr/core/logger.hpp"

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

namespace ltr::web::SelfBinary {

namespace {

// Remonte depuis /Applications/LocalTransfer.app/Contents/MacOS/local_transfer
// → /Applications/LocalTransfer.app. On remonte 3 niveaux.
fs::path locateAppBundle(const fs::path& exe) {
    if (exe.empty()) return {};
    fs::path p = exe;
    for (int i = 0; i < 3 && p.has_parent_path(); ++i) p = p.parent_path();
    if (p.extension() == ".app") return p;
    return {};
}

// Ajoute récursivement un dossier à l'archive zip.
bool addDirRecursive(mz_zip_archive& zip,
                     const fs::path& root,
                     const fs::path& baseInArchive) {
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(
            root, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); ++it) {

        const auto& entry = *it;
        const auto rel = fs::relative(entry.path(), root, ec);
        if (ec) continue;

        const auto archivePath = (baseInArchive / rel).generic_string();

        if (entry.is_directory(ec)) {
            const auto asDir = archivePath + "/";
            if (!mz_zip_writer_add_mem(&zip, asDir.c_str(), nullptr, 0,
                                       MZ_DEFAULT_COMPRESSION)) {
                core::log_error("miniz: add dir failed " + asDir);
                return false;
            }
        } else if (entry.is_regular_file(ec)) {
            const auto abs = entry.path().string();
            if (!mz_zip_writer_add_file(&zip, archivePath.c_str(),
                                        abs.c_str(), nullptr, 0,
                                        MZ_DEFAULT_COMPRESSION)) {
                core::log_error("miniz: add file failed " + archivePath);
                return false;
            }
        }
    }
    return true;
}

} // namespace

std::filesystem::path currentExecutablePath() {
#ifdef __APPLE__
    char buf[1024] = {0};
    uint32_t size = sizeof(buf);
    if (_NSGetExecutablePath(buf, &size) != 0) return {};
    std::error_code ec;
    return fs::canonical(fs::path(buf), ec);
#else
    return {};
#endif
}

std::string suggestedDownloadName() {
    return "LocalTransfer-macOS.zip";
}

std::string mimeType() {
    return "application/zip";
}

bool produceBytes(std::vector<std::uint8_t>& out) {
    const auto exe = currentExecutablePath();
    const auto app = locateAppBundle(exe);
    if (app.empty() || !fs::exists(app)) {
        core::log_warn("SelfBinary: .app introuvable — fallback sur exe nu");
        std::ifstream f(exe, std::ios::binary);
        if (!f) return false;
        f.seekg(0, std::ios::end);
        const auto size = static_cast<std::size_t>(f.tellg());
        f.seekg(0, std::ios::beg);
        out.resize(size);
        if (size > 0) {
            f.read(reinterpret_cast<char*>(out.data()),
                   static_cast<std::streamsize>(size));
        }
        return static_cast<bool>(f);
    }

    mz_zip_archive zip;
    std::memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_heap(&zip, 0, 64 * 1024)) {
        core::log_error("miniz: init_heap failed");
        return false;
    }

    const auto bundleName = app.filename();
    const bool ok = addDirRecursive(zip, app, bundleName);
    if (!ok) {
        mz_zip_writer_end(&zip);
        return false;
    }

    void* data = nullptr;
    std::size_t sz = 0;
    if (!mz_zip_writer_finalize_heap_archive(&zip, &data, &sz)) {
        core::log_error("miniz: finalize failed");
        mz_zip_writer_end(&zip);
        return false;
    }

    out.assign(static_cast<std::uint8_t*>(data),
               static_cast<std::uint8_t*>(data) + sz);
    mz_free(data);
    mz_zip_writer_end(&zip);
    return true;
}

} // namespace ltr::web::SelfBinary
