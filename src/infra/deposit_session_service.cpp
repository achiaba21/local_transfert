#include "ltr/infra/deposit_session_service.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <system_error>

#include <picosha2.h>

#include "ltr/core/logger.hpp"

namespace ltr::infra {

namespace {

std::int64_t systemNowSec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string makeReservationId(const std::string& sessionId,
                              const std::string& fileName,
                              std::size_t index) {
    return "deposit:" + sessionId + ":" + std::to_string(index) + ":" + fileName;
}

} // namespace

DepositSessionService::DepositSessionService(
    DepositSessionRepository& sessionRepo,
    DepositLinkService& linkService,
    TransferQuota& quota,
    DepositReceiptService& receipts,
    DepositHistoryStore& history,
    core::EventBus& bus,
    std::filesystem::path depositsRoot,
    Clock clock)
    : sessionRepo_(sessionRepo),
      linkService_(linkService),
      quota_(quota),
      receipts_(receipts),
      history_(history),
      bus_(bus),
      depositsRoot_(std::move(depositsRoot)),
      clock_(std::move(clock)) {
    if (!clock_) clock_ = systemNowSec;
}

std::string DepositSessionService::isoDate(std::int64_t epochSec) {
    const auto t = static_cast<std::time_t>(epochSec);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[16] = {0};
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    return std::string(buf);
}

std::filesystem::path
DepositSessionService::makeDepositDir(const DepositLink& link,
                                      const DepositSession& session) const {
    const auto labelSafe   = sanitizeForFilesystem(link.label);
    const auto nameSafe    = sanitizeForFilesystem(session.depositorName);
    const auto shortSid    = session.id.substr(0,
        std::min<std::size_t>(6, session.id.size()));
    const auto leaf = isoDate(session.startedAt) + "__"
        + nameSafe + "__" + shortSid;
    return depositsRoot_ / labelSafe / leaf;
}

DepositResult<DepositSession>
DepositSessionService::begin(const std::string& linkToken,
                             const std::string& depositorName,
                             bool consentAccepted) {
    if (depositorName.empty()) {
        return DepositResult<DepositSession>::fail("name_required");
    }
    if (!consentAccepted) {
        return DepositResult<DepositSession>::fail("consent_required");
    }
    auto link = linkService_.findByToken(linkToken);
    if (!link) {
        return DepositResult<DepositSession>::fail("not_found");
    }
    if (link->revoked) {
        return DepositResult<DepositSession>::fail("revoked");
    }
    if (!linkService_.isActive(*link)) {
        return DepositResult<DepositSession>::fail("expired");
    }

    DepositSession s;
    s.id              = makeDepositSessionId();
    s.linkId          = link->id;
    s.depositorName   = depositorName;
    s.consentAccepted = true;
    s.startedAt       = clock_();
    s.status          = DepositSession::Status::Open;

    sessionRepo_.save(s);
    return DepositResult<DepositSession>::success(std::move(s));
}

DepositResult<DepositSessionFile>
DepositSessionService::addFile(const std::string& sessionId,
                               const std::string& fileName,
                               std::uint64_t expectedSize,
                               std::istream& stream) {
    auto sessionOpt = sessionRepo_.findById(sessionId);
    if (!sessionOpt) {
        return DepositResult<DepositSessionFile>::fail("not_found");
    }
    if (sessionOpt->status != DepositSession::Status::Open) {
        return DepositResult<DepositSessionFile>::fail("not_found");
    }
    auto link = linkService_.findById(sessionOpt->linkId);
    if (!link) {
        return DepositResult<DepositSessionFile>::fail("not_found");
    }
    if (!linkService_.isActive(*link)) {
        return DepositResult<DepositSessionFile>::fail("expired");
    }

    // Limites du lien.
    if (link->maxFilesPerDeposit > 0 &&
        static_cast<int>(sessionOpt->files.size()) >= link->maxFilesPerDeposit) {
        return DepositResult<DepositSessionFile>::fail("files_limit");
    }
    if (link->maxBytesPerDeposit > 0 &&
        sessionOpt->totalBytes + expectedSize > link->maxBytesPerDeposit) {
        return DepositResult<DepositSessionFile>::fail("size_limit");
    }

    // Réservation quota.
    const auto reservationId = makeReservationId(
        sessionId, fileName, sessionOpt->files.size());
    const auto decision = quota_.tryReserve(
        reservationId, TransferFlow::HttpUp, expectedSize);
    if (!decision.allowed) {
        return DepositResult<DepositSessionFile>::fail("storage_full");
    }

    // Chemin cible.
    const auto dir = makeDepositDir(*link, *sessionOpt);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        quota_.release(reservationId);
        core::log_error("[deposit] mkdir failed: " + ec.message());
        return DepositResult<DepositSessionFile>::fail("storage_full");
    }
    const auto fileSafe = sanitizeForFilesystem(fileName, 200);
    const auto target = dir / fileSafe;

    // Stream avec hash sha256 calculé au vol.
    picosha2::hash256_one_by_one hasher;
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    if (!out) {
        quota_.release(reservationId);
        return DepositResult<DepositSessionFile>::fail("storage_full");
    }
    std::vector<char> buffer(64 * 1024);
    std::uint64_t actualBytes = 0;
    while (stream.read(buffer.data(),
        static_cast<std::streamsize>(buffer.size())) || stream.gcount() > 0) {
        const auto got = static_cast<std::size_t>(stream.gcount());
        out.write(buffer.data(), static_cast<std::streamsize>(got));
        hasher.process(buffer.begin(), buffer.begin() + got);
        actualBytes += got;
    }
    out.close();
    hasher.finish();
    std::string sha256 = picosha2::get_hash_hex_string(hasher);

    // Commit quota avec taille réelle.
    quota_.commit(reservationId, actualBytes);

    // Mise à jour de la session persistée.
    DepositSessionFile entry;
    entry.name       = fileName;
    entry.size       = actualBytes;
    entry.sha256     = std::move(sha256);
    entry.storedPath = target.string();

    sessionOpt->files.push_back(entry);
    sessionOpt->totalBytes += actualBytes;
    sessionRepo_.save(*sessionOpt);

    {
        std::lock_guard<std::mutex> lock(mu_);
        activeReservations_[sessionId].push_back(reservationId);
    }

    return DepositResult<DepositSessionFile>::success(std::move(entry));
}

DepositResult<DepositReceipt>
DepositSessionService::finalize(const std::string& sessionId) {
    auto sessionOpt = sessionRepo_.findById(sessionId);
    if (!sessionOpt) {
        return DepositResult<DepositReceipt>::fail("not_found");
    }
    if (sessionOpt->status == DepositSession::Status::Finalized) {
        return DepositResult<DepositReceipt>::fail("not_found");
    }
    auto link = linkService_.findById(sessionOpt->linkId);
    if (!link) {
        return DepositResult<DepositReceipt>::fail("not_found");
    }

    const auto now = clock_();
    sessionOpt->finishedAt = now;
    sessionOpt->status     = DepositSession::Status::Finalized;
    sessionRepo_.save(*sessionOpt);

    const auto receipt = receipts_.build(*sessionOpt, *link, now);

    DepositHistory::Entry entry;
    entry.receiptId       = receipt.id;
    entry.sessionId       = sessionOpt->id;
    entry.linkId          = link->id;
    entry.linkLabel       = link->label;
    entry.depositorName   = sessionOpt->depositorName;
    entry.fileCount       = static_cast<int>(sessionOpt->files.size());
    entry.totalBytes      = sessionOpt->totalBytes;
    entry.consentAccepted = sessionOpt->consentAccepted;
    entry.status          = DepositHistory::Status::Finalized;
    entry.startedAt       = sessionOpt->startedAt;
    entry.finishedAt      = sessionOpt->finishedAt;
    history_.append(entry);

    bus_.post(core::DepositReceivedEvent{
        sessionOpt->id, link->id, link->label,
        sessionOpt->depositorName,
        static_cast<int>(sessionOpt->files.size()),
        sessionOpt->totalBytes,
        now,
    });

    {
        std::lock_guard<std::mutex> lock(mu_);
        activeReservations_.erase(sessionId);
    }

    return DepositResult<DepositReceipt>::success(receipt);
}

void DepositSessionService::cancel(const std::string& sessionId) {
    auto sessionOpt = sessionRepo_.findById(sessionId);
    if (!sessionOpt) return;

    // Release réservations en cours (no-op pour les commits déjà faits).
    std::vector<std::string> reservations;
    {
        std::lock_guard<std::mutex> lock(mu_);
        const auto it = activeReservations_.find(sessionId);
        if (it != activeReservations_.end()) {
            reservations = std::move(it->second);
            activeReservations_.erase(it);
        }
    }
    for (const auto& r : reservations) quota_.release(r);

    // Cleanup fichiers déjà écrits.
    std::error_code ec;
    for (const auto& f : sessionOpt->files) {
        std::filesystem::remove(f.storedPath, ec);
    }

    sessionOpt->status     = DepositSession::Status::Cancelled;
    sessionOpt->finishedAt = clock_();
    sessionRepo_.save(*sessionOpt);
}

std::optional<DepositSession>
DepositSessionService::get(const std::string& sessionId) {
    return sessionRepo_.findById(sessionId);
}

void DepositSessionService::gcAbandoned(std::int64_t ttlSeconds) {
    const auto cutoff = clock_() - ttlSeconds;
    for (const auto& s : sessionRepo_.loadAll()) {
        if (s.status == DepositSession::Status::Open &&
            s.startedAt < cutoff) {
            cancel(s.id);
        }
    }
}

} // namespace ltr::infra
