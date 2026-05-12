#include "ltr/infra/quota_service.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <limits>
#include <system_error>

#include <nlohmann/json.hpp>

#include "ltr/core/logger.hpp"

namespace ltr::infra {

namespace {

std::int64_t systemNowSec() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// Howard Hinnant, public domain. Days since 1970-01-01.
std::int64_t daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<int>(doe) - 719468;
}

std::tm utcTm(std::int64_t epochSec) {
    const auto t = static_cast<std::time_t>(epochSec);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    return tm;
}

std::uint64_t saturatingAdd(std::uint64_t a, std::uint64_t b) {
    const auto max = std::numeric_limits<std::uint64_t>::max();
    if (max - a < b) return max;
    return a + b;
}

} // namespace

const char* transferFlowToStr(TransferFlow flow) {
    switch (flow) {
        case TransferFlow::TcpOut:   return "tcp-out";
        case TransferFlow::TcpIn:    return "tcp-in";
        case TransferFlow::HttpUp:   return "http-up";
        case TransferFlow::HttpDown: return "http-down";
    }
    return "unknown";
}

std::string quotaPeriodUtc(std::int64_t epochSec) {
    const auto tm = utcTm(epochSec);
    char buf[16] = {0};
    std::snprintf(buf, sizeof(buf), "%04d-%02d",
                  tm.tm_year + 1900, tm.tm_mon + 1);
    return std::string(buf);
}

std::int64_t quotaNextResetUtc(std::int64_t epochSec) {
    const auto tm = utcTm(epochSec);
    int year = tm.tm_year + 1900;
    unsigned month = static_cast<unsigned>(tm.tm_mon + 2);
    if (month > 12) {
        month = 1;
        ++year;
    }
    return daysFromCivil(year, month, 1) * 24LL * 3600LL;
}

JsonQuotaRepository::JsonQuotaRepository(std::filesystem::path path)
    : path_(std::move(path)) {}

std::optional<QuotaUsage> JsonQuotaRepository::load() const {
    std::error_code ec;
    if (!std::filesystem::exists(path_, ec)) return std::nullopt;

    try {
        std::ifstream in(path_);
        const auto j = nlohmann::json::parse(in);
        QuotaUsage usage;
        usage.period = j.value("period", std::string{});
        usage.usedBytes = j.value("usedBytes", std::uint64_t{0});
        if (usage.period.empty()) return std::nullopt;
        return usage;
    } catch (const std::exception& e) {
        core::log_warn(std::string("Quota usage parse failed: ") + e.what());
        return std::nullopt;
    }
}

void JsonQuotaRepository::save(const QuotaUsage& usage) const {
    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);

    nlohmann::json j;
    j["period"] = usage.period;
    j["usedBytes"] = usage.usedBytes;

    const auto tmp = path_.string() + ".tmp";
    std::ofstream out(tmp, std::ios::trunc);
    if (!out) return;
    out << j.dump(2);
    out.close();
    std::filesystem::rename(tmp, path_, ec);
}

QuotaService::QuotaService(QuotaRepository& repository,
                           PolicyService& policyService,
                           Clock clock)
    : repository_(repository),
      policyService_(policyService),
      clock_(std::move(clock)) {
    if (!clock_) clock_ = systemNowSec;
}

QuotaDecision QuotaService::tryReserve(const std::string& reservationId,
                                       TransferFlow flow,
                                       std::uint64_t expectedBytes) {
    std::lock_guard<std::mutex> lock(mu_);
    const auto now = clock_();
    ensureLoaded();
    ensureCurrentPeriod(now);

    const auto& policy = policyService_.current();
    QuotaDecision decision;
    decision.period = usage_.period;
    decision.requestedBytes = expectedBytes;
    decision.usedBytes = usage_.usedBytes;
    decision.reservedBytes = reservedBytesLocked();
    decision.limitBytes = policy.quota.monthlyBytes;
    decision.resetAt = quotaNextResetUtc(now);

    if (!policy.quota.enabled || policy.quota.monthlyBytes == 0) {
        decision.allowed = true;
        return decision;
    }

    const auto totalCommittedAndReserved = saturatingAdd(
        usage_.usedBytes, decision.reservedBytes);
    const auto afterReservation = saturatingAdd(
        totalCommittedAndReserved, expectedBytes);

    if (afterReservation > policy.quota.monthlyBytes &&
        policy.quota.enforcement == QuotaEnforcement::HardBlock) {
        decision.allowed = false;
        decision.reason = "quota_exceeded";
        return decision;
    }

    reservations_[reservationId] = Reservation{
        usage_.period, flow, expectedBytes};
    decision.allowed = true;
    decision.reservedBytes = reservedBytesLocked();
    return decision;
}

void QuotaService::commit(const std::string& reservationId,
                          std::uint64_t actualBytes) {
    std::lock_guard<std::mutex> lock(mu_);
    const auto now = clock_();
    ensureLoaded();
    ensureCurrentPeriod(now);

    const auto& policy = policyService_.current();
    if (!policy.quota.enabled || policy.quota.monthlyBytes == 0) {
        reservations_.erase(reservationId);
        return;
    }

    auto it = reservations_.find(reservationId);
    if (it != reservations_.end() && it->second.period != usage_.period) {
        reservations_.erase(it);
    } else if (it != reservations_.end()) {
        reservations_.erase(it);
    }

    usage_.usedBytes = saturatingAdd(usage_.usedBytes, actualBytes);
    repository_.save(usage_);
}

void QuotaService::release(const std::string& reservationId) {
    std::lock_guard<std::mutex> lock(mu_);
    reservations_.erase(reservationId);
}

QuotaUsage QuotaService::usageSnapshot() {
    std::lock_guard<std::mutex> lock(mu_);
    ensureLoaded();
    ensureCurrentPeriod(clock_());
    return usage_;
}

std::uint64_t QuotaService::reservedBytesSnapshot() {
    std::lock_guard<std::mutex> lock(mu_);
    ensureLoaded();
    ensureCurrentPeriod(clock_());
    return reservedBytesLocked();
}

void QuotaService::ensureLoaded() {
    if (loaded_) return;
    if (auto usage = repository_.load()) usage_ = *usage;
    loaded_ = true;
}

void QuotaService::ensureCurrentPeriod(std::int64_t now) {
    const auto current = quotaPeriodUtc(now);
    if (usage_.period == current) return;
    usage_.period = current;
    usage_.usedBytes = 0;
    reservations_.clear();
    repository_.save(usage_);
}

std::uint64_t QuotaService::reservedBytesLocked() const {
    std::uint64_t total = 0;
    for (const auto& kv : reservations_) {
        if (kv.second.period == usage_.period) {
            total = saturatingAdd(total, kv.second.bytes);
        }
    }
    return total;
}

} // namespace ltr::infra
