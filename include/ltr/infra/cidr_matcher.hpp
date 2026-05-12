#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace ltr::infra {

// Représentation interne d'un CIDR parsé. POD trivial.
struct CidrRange {
    bool                       isV4{true};
    std::uint32_t              v4Addr{0};
    std::uint32_t              v4Mask{0xFFFFFFFFu};
    std::array<std::uint8_t,16> v6Addr{};
    int                        v6PrefixBits{128};
};

// Parse une notation CIDR ou un host nu :
//   "192.168.1.0/24"  → IPv4 CIDR /24
//   "192.168.1.42"    → IPv4 single host (/32)
//   "::1/128"         → IPv6 CIDR /128
//   "::1"             → IPv6 single (/128)
// Retourne std::nullopt si la chaîne est invalide.
std::optional<CidrRange> parseCidr(const std::string& s);

// True si `ip` (forme texte, IPv4 ou IPv6) appartient à `range`.
bool matchCidr(const CidrRange& range, const std::string& ip);

} // namespace ltr::infra
