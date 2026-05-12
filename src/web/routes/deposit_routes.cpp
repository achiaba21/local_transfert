#include "ltr/web/routes/deposit_routes.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "ltr/core/logger.hpp"
#include "ltr/infra/deposit_link_service.hpp"
#include "ltr/infra/deposit_receipt.hpp"
#include "ltr/infra/deposit_session_service.hpp"
#include "ltr/web/routes/multi_server.hpp"
#include "ltr/web/web_service.hpp"

// Assets embedded — pas de fs::ifstream au runtime, fonctionne aussi
// quand le binaire est lancé hors du dossier projet.
#include "ltr/web/assets/deposit_html.hpp"
#include "ltr/web/assets/deposit_expired_html.hpp"
#include "ltr/web/assets/deposit_receipt_html.hpp"

namespace ltr::web::routes {

namespace {

void serveEmbeddedHtml(std::string_view bytes,
                       std::string_view mime,
                       httplib::Response& res) {
    res.set_header("Cache-Control", "no-store");
    res.set_content(bytes.data(), bytes.size(), std::string(mime).c_str());
}

std::string mapReasonToMessage(const std::string& reason) {
    if (reason == "name_required")    return "Veuillez saisir votre nom.";
    if (reason == "consent_required") return "Le consentement est requis.";
    if (reason == "expired")          return "Ce lien n'est plus actif.";
    if (reason == "revoked")          return "Ce lien n'est plus actif.";
    if (reason == "not_found")        return "Ce lien n'est plus actif.";
    if (reason == "files_limit")      return "Trop de fichiers pour ce dépôt.";
    if (reason == "size_limit")       return "Ce dépôt dépasse la taille autorisée.";
    if (reason == "storage_full")     return "Ce dépôt ne peut pas être accepté pour le moment.";
    return "Une erreur est survenue. Réessayez.";
}

} // namespace

void registerDeposit(WebService& svc) {
    auto server = routes::routerOf(svc);

    using namespace ltr::web::assets;
    server.Get(R"(/deposit/([A-Za-z0-9_\-]+))",
        [&svc](const httplib::Request& req, httplib::Response& res) {
            const auto token = req.matches[1].str();
            auto* links = svc.depositLinks();
            if (!links) {
                res.status = 500;
                serveEmbeddedHtml(DepositExpiredHtml,
                                   DepositExpiredHtmlMime, res);
                return;
            }
            auto link = links->findByToken(token);
            if (!link || !links->isActive(*link)) {
                serveEmbeddedHtml(DepositExpiredHtml,
                                   DepositExpiredHtmlMime, res);
                return;
            }
            serveEmbeddedHtml(DepositHtml, DepositHtmlMime, res);
        });

    server.Get("/api/deposit/info",
        [&svc](const httplib::Request& req, httplib::Response& res) {
            const auto token = req.has_param("token")
                ? req.get_param_value("token") : std::string{};
            auto* links = svc.depositLinks();
            if (!links) {
                res.status = 503;
                res.set_content("{\"error\":\"deposits_disabled\"}",
                                "application/json");
                return;
            }
            auto link = links->findByToken(token);
            if (!link || !links->isActive(*link)) {
                res.status = 404;
                res.set_content("{\"error\":\"link_inactive\"}",
                                "application/json");
                return;
            }
            nlohmann::json j;
            j["label"]          = link->label;
            j["hostName"]       = svc.self().name;
            j["consentText"]    = link->consentText;
            j["maxBytes"]       = link->maxBytesPerDeposit;
            j["maxFiles"]       = link->maxFilesPerDeposit;
            res.set_header("Cache-Control", "no-store");
            res.set_content(j.dump(), "application/json; charset=utf-8");
        });

    server.Post("/api/deposit/begin",
        [&svc](const httplib::Request& req, httplib::Response& res) {
            const auto token = req.has_param("token")
                ? req.get_param_value("token") : std::string{};
            auto* sessions = svc.depositSessions();
            if (!sessions) {
                res.status = 503;
                res.set_content("{\"error\":\"deposits_disabled\"}",
                                "application/json");
                return;
            }
            std::string name;
            bool consent = false;
            try {
                const auto j = nlohmann::json::parse(req.body);
                name    = j.value("name", std::string{});
                consent = j.value("consent", false);
            } catch (...) {
                res.status = 400;
                res.set_content("{\"error\":\"bad_json\"}", "application/json");
                return;
            }

            const auto result = sessions->begin(token, name, consent);
            if (!result.ok) {
                nlohmann::json j;
                j["error"]   = result.reason;
                j["message"] = mapReasonToMessage(result.reason);
                res.status = (result.reason == "not_found" ||
                              result.reason == "expired" ||
                              result.reason == "revoked") ? 404 : 400;
                res.set_content(j.dump(), "application/json");
                return;
            }
            nlohmann::json j;
            j["sessionId"] = result.value.id;
            res.set_content(j.dump(), "application/json; charset=utf-8");
        });

    server.Post("/api/deposit/upload",
        [&svc](const httplib::Request& req, httplib::Response& res,
               const httplib::ContentReader& content_reader) {
            const auto sessionId = req.has_param("session")
                ? req.get_param_value("session") : std::string{};
            auto* sessions = svc.depositSessions();
            if (!sessions) {
                res.status = 503;
                res.set_content("{\"error\":\"deposits_disabled\"}",
                                "application/json");
                return;
            }
            if (!req.is_multipart_form_data()) {
                res.status = 400;
                res.set_content("{\"error\":\"not_multipart\"}",
                                "application/json");
                return;
            }

            // Collect multipart parts (small files first ; gros fichiers
            // bufferisés dans /tmp via httplib).
            httplib::MultipartFormDataItems items;
            std::string currentName;
            std::string currentFilename;
            std::stringstream currentBuffer;
            bool inFile = false;

            const bool ok = content_reader(
                [&](const httplib::MultipartFormData& file) {
                    currentName     = file.name;
                    currentFilename = file.filename;
                    currentBuffer.str("");
                    currentBuffer.clear();
                    inFile = (file.name == "file");
                    return true;
                },
                [&](const char* data, std::size_t size) {
                    if (inFile) currentBuffer.write(data,
                        static_cast<std::streamsize>(size));
                    return true;
                });

            if (!ok || !inFile || currentFilename.empty()) {
                res.status = 400;
                res.set_content("{\"error\":\"no_file\"}", "application/json");
                return;
            }

            const auto bodyStr = currentBuffer.str();
            std::istringstream stream(bodyStr);
            const auto result = sessions->addFile(
                sessionId, currentFilename, bodyStr.size(), stream);
            if (!result.ok) {
                nlohmann::json j;
                j["error"]   = result.reason;
                j["message"] = mapReasonToMessage(result.reason);
                res.status = (result.reason == "size_limit" ||
                              result.reason == "files_limit") ? 413 :
                             (result.reason == "storage_full") ? 403 : 400;
                res.set_content(j.dump(), "application/json");
                return;
            }
            nlohmann::json j;
            j["name"]   = result.value.name;
            j["size"]   = result.value.size;
            j["sha256"] = result.value.sha256;
            res.set_content(j.dump(), "application/json");
        });

    server.Post("/api/deposit/finalize",
        [&svc](const httplib::Request& req, httplib::Response& res) {
            const auto sessionId = req.has_param("session")
                ? req.get_param_value("session") : std::string{};
            auto* sessions = svc.depositSessions();
            auto* receipts = svc.depositReceipts();
            if (!sessions || !receipts) {
                res.status = 503;
                res.set_content("{\"error\":\"deposits_disabled\"}",
                                "application/json");
                return;
            }
            const auto result = sessions->finalize(sessionId);
            if (!result.ok) {
                nlohmann::json j;
                j["error"]   = result.reason;
                j["message"] = mapReasonToMessage(result.reason);
                res.status = 404;
                res.set_content(j.dump(), "application/json");
                return;
            }
            // Persiste le reçu sur disque pour lecture ultérieure
            // (downloadDir/Deposits/.receipts/<id>.json).
            const auto receipt = result.value;
            auto receiptsDir = svc.downloadDir() / "Deposits" / ".receipts";
            std::error_code ec;
            std::filesystem::create_directories(receiptsDir, ec);
            const auto receiptPath = receiptsDir / (receipt.id + ".json");
            {
                std::ofstream out(receiptPath, std::ios::trunc);
                if (out) out << receipts->toJson(receipt);
            }
            nlohmann::json j;
            j["receiptId"]   = receipt.id;
            j["downloadUrl"] = "/api/deposit/receipt/" + receipt.id;
            j["fileCount"]   = static_cast<int>(receipt.files.size());
            j["totalBytes"]  = receipt.totalBytes;
            res.set_content(j.dump(), "application/json; charset=utf-8");
        });

    server.Get(R"(/api/deposit/receipt/([a-f0-9]+))",
        [&svc](const httplib::Request& req, httplib::Response& res) {
            const auto id = req.matches[1].str();
            const auto path = svc.downloadDir() / "Deposits" / ".receipts"
                            / (id + ".json");
            std::ifstream in(path);
            if (!in) {
                res.status = 404;
                res.set_content("{\"error\":\"not_found\"}", "application/json");
                return;
            }
            std::ostringstream ss;
            ss << in.rdbuf();
            res.set_header("Content-Disposition",
                "attachment; filename=\"deposit-receipt-" + id + ".json\"");
            res.set_content(ss.str(), "application/json; charset=utf-8");
        });
}

} // namespace ltr::web::routes
