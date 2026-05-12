#include "ltr/infra/audit_export_service.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace ltr::infra;

int main() {
    TransferHistory::Entry a;
    a.sessionId = "sid-1";
    a.peerDeviceId = "dev-1";
    a.peerName = "Client, Test";
    a.kind = TransferHistory::Kind::HttpUp;
    a.fileCount = 2;
    a.totalBytes = 1234;
    a.status = TransferHistory::Status::Ok;
    a.startedAt = 10;
    a.finishedAt = 20;

    TransferHistory::Entry b;
    b.sessionId = "sid-2";
    b.peerDeviceId = "dev-2";
    b.peerName = "Office";
    b.kind = TransferHistory::Kind::HttpDown;
    b.fileCount = 1;
    b.totalBytes = 99;
    b.status = TransferHistory::Status::Failed;
    b.error = "quota_exceeded";

    AuditExportService exporter;
    const std::vector<TransferHistory::Entry> entries{a, b};
    const auto csv = exporter.exportCsv(entries);
    assert(csv.find("sessionId,peerDeviceId,peerName,direction,kind") == 0);
    assert(csv.find("\"Client, Test\",in,http-up") != std::string::npos);
    assert(csv.find("Office,out,http-down") != std::string::npos);

    const auto json = exporter.exportJson(entries);
    assert(json.find("\"direction\": \"in\"") != std::string::npos);
    assert(json.find("\"kind\": \"http-down\"") != std::string::npos);
    assert(json.find("\"error\": \"quota_exceeded\"") != std::string::npos);

    std::cout << "test_audit_export_service: OK\n";
    return 0;
}
