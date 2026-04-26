#include "ltr/web/self_binary.hpp"

#include <fstream>

#include "ltr/core/logger.hpp"

#include <unistd.h>
#include <climits>

namespace ltr::web::SelfBinary {

std::filesystem::path currentExecutablePath() {
    char buf[PATH_MAX] = {0};
    const ssize_t n = readlink("/proc/self/exe", buf, PATH_MAX - 1);
    if (n <= 0) return {};
    return std::filesystem::path(std::string(buf, static_cast<std::size_t>(n)));
}

std::string suggestedDownloadName() {
    return "LocalTransfer-Linux";
}

std::string mimeType() {
    return "application/octet-stream";
}

bool produceBytes(std::vector<std::uint8_t>& out) {
    const auto path = currentExecutablePath();
    if (path.empty()) return false;

    std::ifstream f(path, std::ios::binary);
    if (!f) {
        core::log_error("SelfBinary: open failed: " + path.string());
        return false;
    }
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

} // namespace ltr::web::SelfBinary
