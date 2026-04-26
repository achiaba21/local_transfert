#include "ltr/web/self_binary.hpp"

#include <fstream>

#include "ltr/core/logger.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace ltr::web::SelfBinary {

std::filesystem::path currentExecutablePath() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {0};
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n == MAX_PATH) return {};
    return std::filesystem::path(buf);
#else
    return {};
#endif
}

std::string suggestedDownloadName() {
    return "LocalTransfer-Windows.exe";
}

std::string mimeType() {
    return "application/vnd.microsoft.portable-executable";
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
