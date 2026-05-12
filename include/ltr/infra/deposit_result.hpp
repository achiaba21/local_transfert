#pragma once

#include <string>
#include <utility>

namespace ltr::infra {

// Result<T> léger — pas d'exception dans les services dépôt.
// `ok=true` ⇒ `value` valide. `ok=false` ⇒ `reason` ∈ {
//   "name_required", "consent_required", "expired", "revoked",
//   "files_limit", "size_limit", "storage_full", "upsell_required",
//   "not_found", "unknown" }.
template <class T>
struct DepositResult {
    bool        ok{false};
    std::string reason;
    T           value{};

    static DepositResult success(T v) {
        DepositResult r;
        r.ok = true;
        r.value = std::move(v);
        return r;
    }
    static DepositResult fail(std::string why) {
        DepositResult r;
        r.ok = false;
        r.reason = std::move(why);
        return r;
    }
};

struct Unit {};

using DepositVoidResult = DepositResult<Unit>;

} // namespace ltr::infra
