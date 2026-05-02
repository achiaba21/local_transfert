#include "ltr/infra/filesystem_service.hpp"

#include "ltr/core/logger.hpp"

#include <system_error>

namespace ltr::infra {

namespace {

std::string toPosix(const std::filesystem::path& p) {
    std::string s = p.generic_string();
    return s;
}

} // namespace

std::vector<FileEntry> FilesystemService::enumerate(
    const std::vector<std::filesystem::path>& inputs) {
    std::vector<FileEntry> out;

    for (const auto& in : inputs) {
        std::error_code ec;
        if (!std::filesystem::exists(in, ec)) {
            core::log_warn("Chemin inexistant : " + in.string());
            continue;
        }

        if (std::filesystem::is_regular_file(in, ec)) {
            FileEntry e;
            e.absolutePath = in;
            e.meta.relativePath = toPosix(in.filename());
            e.meta.size = std::filesystem::file_size(in, ec);
            out.push_back(std::move(e));
            continue;
        }

        if (std::filesystem::is_directory(in, ec)) {
            // V1.3.1 : `path::filename()` retourne "" si le chemin se
            // termine par un séparateur ("/Users/foo/MyFolder/" → "").
            // Selon la source du path (picker macOS, drag&drop, glob…),
            // un trailing slash peut apparaître → folder name perdu →
            // fichiers déposés à plat chez le receveur. On retombe sur
            // parent_path().filename() dans ce cas.
            auto base = in.filename();
            if (base.empty()) base = in.parent_path().filename();
            for (const auto& entry :
                 std::filesystem::recursive_directory_iterator(in, ec)) {
                if (!entry.is_regular_file(ec)) continue;
                FileEntry e;
                e.absolutePath = entry.path();
                const auto rel =
                    base / std::filesystem::relative(entry.path(), in, ec);
                e.meta.relativePath = toPosix(rel);
                e.meta.size = entry.file_size(ec);
                out.push_back(std::move(e));
            }
        }
    }
    return out;
}

std::filesystem::path FilesystemService::uniqueTargetPath(
    const std::filesystem::path& targetDir,
    const std::string& relativePath) {
    namespace fs = std::filesystem;
    fs::path candidate = targetDir / fs::path(relativePath).lexically_normal();

    std::error_code ec;
    fs::create_directories(candidate.parent_path(), ec);

    if (!fs::exists(candidate, ec)) return candidate;

    const auto stem = candidate.stem().string();
    const auto ext  = candidate.extension().string();
    for (int i = 1; i < 10000; ++i) {
        fs::path alt = candidate.parent_path()
            / (stem + " (" + std::to_string(i) + ")" + ext);
        if (!fs::exists(alt, ec)) return alt;
    }
    return candidate; // dernier recours : on écrase
}

bool FilesystemService::hasSpace(const std::filesystem::path& path,
                                 std::uint64_t requiredBytes) {
    std::error_code ec;
    auto info = std::filesystem::space(path, ec);
    if (ec) return true; // en cas de doute, on laisse passer
    return info.available >= requiredBytes;
}

} // namespace ltr::infra
