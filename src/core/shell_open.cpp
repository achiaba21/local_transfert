#include "ltr/core/shell_open.hpp"

#include "ltr/core/logger.hpp"

#include <cstdlib>
#include <string>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <shellapi.h>
#endif

namespace ltr::core {

namespace {

#if !defined(_WIN32)
// Shell-escape POSIX : entoure de '...' et double tout caractère '.
// Évite l'injection de commande via noms de fichiers hostiles.
std::string shellEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('\'');
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else           out.push_back(c);
    }
    out.push_back('\'');
    return out;
}
#endif

} // namespace

bool openInFileManager(const std::filesystem::path& path) {
    const auto s = path.string();

#if defined(__APPLE__)
    const auto cmd = "open " + shellEscape(s) + " >/dev/null 2>&1 &";
    const int rc = std::system(cmd.c_str());
    if (rc != 0) {
        log_warn("openInFileManager: open a renvoyé " + std::to_string(rc));
    }
    return rc == 0;

#elif defined(_WIN32)
    const std::wstring w(s.begin(), s.end());
    const auto res = ShellExecuteW(nullptr, L"open", w.c_str(),
                                    nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(res) <= 32) {
        log_warn("openInFileManager: ShellExecuteW a échoué");
        return false;
    }
    return true;

#else
    const auto cmd = "xdg-open " + shellEscape(s) + " >/dev/null 2>&1 &";
    const int rc = std::system(cmd.c_str());
    if (rc != 0) {
        log_warn("openInFileManager: xdg-open a renvoyé " +
                 std::to_string(rc));
    }
    return rc == 0;
#endif
}

} // namespace ltr::core
