#include "ltr/infra/audit_export_service.hpp"

#include <sstream>

#include <nlohmann/json.hpp>

namespace ltr::infra {

namespace {

std::string csvEscape(const std::string& value) {
    bool needsQuotes = false;
    for (char c : value) {
        if (c == '"' || c == ',' || c == '\n' || c == '\r') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) return value;

    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (char c : value) {
        if (c == '"') out.push_back('"');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

nlohmann::json entryToJson(const TransferHistory::Entry& e) {
    return {
        {"sessionId", e.sessionId},
        {"peerDeviceId", e.peerDeviceId},
        {"peerName", e.peerName},
        {"direction", transferDirectionForKind(e.kind)},
        {"kind", TransferHistory::kindToStr(e.kind)},
        {"fileCount", e.fileCount},
        {"totalBytes", e.totalBytes},
        {"status", TransferHistory::statusToStr(e.status)},
        {"startedAt", e.startedAt},
        {"finishedAt", e.finishedAt},
        {"error", e.error},
    };
}

nlohmann::json depositEntryToJson(const DepositHistory::Entry& e) {
    return {
        {"receiptId", e.receiptId},
        {"sessionId", e.sessionId},
        {"linkId", e.linkId},
        {"linkLabel", e.linkLabel},
        {"depositorName", e.depositorName},
        {"direction", "deposit-in"},
        {"kind", "deposit"},
        {"fileCount", e.fileCount},
        {"totalBytes", e.totalBytes},
        {"consentAccepted", e.consentAccepted},
        {"status", DepositHistory::statusToStr(e.status)},
        {"startedAt", e.startedAt},
        {"finishedAt", e.finishedAt},
    };
}

} // namespace

std::string transferDirectionForKind(TransferHistory::Kind kind) {
    switch (kind) {
        case TransferHistory::Kind::TcpOut:
        case TransferHistory::Kind::HttpDown:
            return "out";
        case TransferHistory::Kind::TcpIn:
        case TransferHistory::Kind::HttpUp:
            return "in";
    }
    return "unknown";
}

std::string AuditExportService::exportJson(
    const std::vector<TransferHistory::Entry>& entries) const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& e : entries) arr.push_back(entryToJson(e));
    nlohmann::json root;
    root["transfers"] = arr;
    return root.dump(2);
}

std::string AuditExportService::exportCsv(
    const std::vector<TransferHistory::Entry>& entries) const {
    std::ostringstream out;
    out << "sessionId,peerDeviceId,peerName,direction,kind,fileCount,"
           "totalBytes,status,startedAt,finishedAt,error\n";
    for (const auto& e : entries) {
        out << csvEscape(e.sessionId) << ','
            << csvEscape(e.peerDeviceId) << ','
            << csvEscape(e.peerName) << ','
            << transferDirectionForKind(e.kind) << ','
            << TransferHistory::kindToStr(e.kind) << ','
            << e.fileCount << ','
            << e.totalBytes << ','
            << TransferHistory::statusToStr(e.status) << ','
            << e.startedAt << ','
            << e.finishedAt << ','
            << csvEscape(e.error) << '\n';
    }
    return out.str();
}

std::string AuditExportService::exportJson(
    const std::vector<TransferHistory::Entry>& transfers,
    const std::vector<DepositHistory::Entry>& deposits) const {
    nlohmann::json tArr = nlohmann::json::array();
    for (const auto& e : transfers) tArr.push_back(entryToJson(e));
    nlohmann::json dArr = nlohmann::json::array();
    for (const auto& e : deposits) dArr.push_back(depositEntryToJson(e));
    nlohmann::json root;
    root["transfers"] = tArr;
    root["deposits"]  = dArr;
    return root.dump(2);
}

std::string AuditExportService::exportCsv(
    const std::vector<TransferHistory::Entry>& transfers,
    const std::vector<DepositHistory::Entry>& deposits) const {
    std::ostringstream out;
    out << "type,sessionOrReceiptId,peerOrDepositor,linkLabel,direction,"
           "kind,fileCount,totalBytes,status,startedAt,finishedAt,error\n";
    for (const auto& e : transfers) {
        out << "transfer,"
            << csvEscape(e.sessionId) << ','
            << csvEscape(e.peerName) << ','
            << "" << ','
            << transferDirectionForKind(e.kind) << ','
            << TransferHistory::kindToStr(e.kind) << ','
            << e.fileCount << ','
            << e.totalBytes << ','
            << TransferHistory::statusToStr(e.status) << ','
            << e.startedAt << ','
            << e.finishedAt << ','
            << csvEscape(e.error) << '\n';
    }
    for (const auto& e : deposits) {
        out << "deposit,"
            << csvEscape(e.receiptId) << ','
            << csvEscape(e.depositorName) << ','
            << csvEscape(e.linkLabel) << ','
            << "in" << ','
            << "deposit" << ','
            << e.fileCount << ','
            << e.totalBytes << ','
            << DepositHistory::statusToStr(e.status) << ','
            << e.startedAt << ','
            << e.finishedAt << ','
            << "" << '\n';
    }
    return out.str();
}

} // namespace ltr::infra
