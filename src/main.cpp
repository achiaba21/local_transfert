#include "ltr/core/logger.hpp"
#include "ltr/ui/ui_app.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <typeinfo>

int main(int /*argc*/, char** /*argv*/) {
    ltr::core::Logger::instance().setLevel(ltr::core::LogLevel::Debug);

    // V1.1.1 : fichier de log stable pour analyse offline.
    // macOS/Linux : /tmp/localtransfer.log (chemin direct, connu, append).
    // Windows : %TEMP%\localtransfer.log.
    std::filesystem::path logPath;
#ifdef _WIN32
    if (const char* win = std::getenv("TEMP")) logPath = win;
    else logPath = ".";
#else
    logPath = "/tmp";
#endif
    logPath /= "localtransfer.log";
    ltr::core::Logger::instance().openFile(logPath.string());

    ltr::core::log_info("LocalTransfer v0.1 démarre.");
    ltr::core::log_info(std::string("Log file: ") + logPath.string());

    try {
        ltr::core::log_debug("Création de UIApp...");
        ltr::ui::UIApp app;
        ltr::core::log_debug("UIApp créée, lancement de run()...");
        return app.run();
    } catch (const std::exception& e) {
        ltr::core::log_error(std::string("Exception std : ") + typeid(e).name()
                             + " — " + e.what());
        return 1;
    } catch (...) {
        std::exception_ptr p = std::current_exception();
        if (p) {
            try { std::rethrow_exception(p); }
            catch (const std::exception& e) {
                ltr::core::log_error(std::string("Exception non-std : ") +
                                     typeid(e).name() + " — " + e.what());
            }
            catch (...) {
                ltr::core::log_error("Exception non-std, type inconnu (probablement NSException ObjC).");
            }
        } else {
            ltr::core::log_error("Exception inconnue — arrêt.");
        }
        return 2;
    }
}
