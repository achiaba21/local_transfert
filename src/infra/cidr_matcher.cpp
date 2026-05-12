#include "ltr/infra/cidr_matcher.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <vector>

namespace ltr::infra {

namespace {

bool parseIPv4Numeric(const std::string& s, std::uint32_t& out) {
    int parts[4] = {0};
    int idx = 0;
    int current = -1;
    for (char c : s) {
        if (c == '.') {
            if (current < 0 || current > 255 || idx >= 3) return false;
            parts[idx++] = current;
            current = -1;
        } else if (c >= '0' && c <= '9') {
            current = (current < 0 ? 0 : current) * 10 + (c - '0');
            if (current > 999) return false;
        } else {
            return false;
        }
    }
    if (idx != 3 || current < 0 || current > 255) return false;
    parts[3] = current;
    out = (std::uint32_t(parts[0]) << 24) | (std::uint32_t(parts[1]) << 16)
        | (std::uint32_t(parts[2]) <<  8) |  std::uint32_t(parts[3]);
    return true;
}

bool parseIPv6Numeric(const std::string& s, std::array<std::uint8_t,16>& out) {
    out.fill(0);
    std::vector<std::uint16_t> head;
    std::vector<std::uint16_t> tail;
    bool sawDoubleColon = false;
    std::string token;
    bool inTail = false;

    auto flush = [&](bool toTail) -> bool {
        if (token.empty()) return true;
        if (token.size() > 4) return false;
        std::uint16_t v = 0;
        for (char c : token) {
            v <<= 4;
            if (c >= '0' && c <= '9') v |= (c - '0');
            else if (c >= 'a' && c <= 'f') v |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (c - 'A' + 10);
            else return false;
        }
        if (toTail) tail.push_back(v); else head.push_back(v);
        token.clear();
        return true;
    };

    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == ':') {
            if (!flush(inTail)) return false;
            if (i + 1 < s.size() && s[i + 1] == ':') {
                if (sawDoubleColon) return false;
                sawDoubleColon = true;
                inTail = true;
                ++i;
            }
        } else {
            token.push_back(c);
        }
    }
    if (!flush(inTail)) return false;

    if (head.size() + tail.size() > 8) return false;
    if (!sawDoubleColon && head.size() + tail.size() != 8) return false;

    std::vector<std::uint16_t> words(8, 0);
    for (std::size_t i = 0; i < head.size(); ++i) words[i] = head[i];
    for (std::size_t i = 0; i < tail.size(); ++i) {
        words[8 - tail.size() + i] = tail[i];
    }
    for (int i = 0; i < 8; ++i) {
        out[i * 2 + 0] = static_cast<std::uint8_t>(words[i] >> 8);
        out[i * 2 + 1] = static_cast<std::uint8_t>(words[i] & 0xFF);
    }
    return true;
}

} // namespace

std::optional<CidrRange> parseCidr(const std::string& s) {
    if (s.empty()) return std::nullopt;
    std::string addr = s;
    bool hasSlash = false;
    int  prefix   = 0;
    const auto slash = s.find('/');
    if (slash != std::string::npos) {
        hasSlash = true;
        addr = s.substr(0, slash);
        const auto rest = s.substr(slash + 1);
        if (rest.empty()) return std::nullopt;
        for (char c : rest) {
            if (c < '0' || c > '9') return std::nullopt;
        }
        try {
            prefix = std::stoi(rest);
        } catch (...) {
            return std::nullopt;
        }
    }
    const bool isV6 = addr.find(':') != std::string::npos;
    CidrRange r;
    r.isV4 = !isV6;
    if (!isV6) {
        std::uint32_t v = 0;
        if (!parseIPv4Numeric(addr, v)) return std::nullopt;
        if (!hasSlash) prefix = 32;
        if (prefix < 0 || prefix > 32) return std::nullopt;
        r.v4Addr = v;
        r.v4Mask = (prefix == 0) ? 0u
                                 : (0xFFFFFFFFu << (32 - prefix));
        r.v4Addr &= r.v4Mask;
        return r;
    } else {
        std::array<std::uint8_t,16> a{};
        if (!parseIPv6Numeric(addr, a)) return std::nullopt;
        if (!hasSlash) prefix = 128;
        if (prefix < 0 || prefix > 128) return std::nullopt;
        // masquer
        for (int i = 0; i < 16; ++i) {
            const int bitStart = i * 8;
            if (bitStart >= prefix) a[i] = 0;
            else if (bitStart + 8 <= prefix) { /* keep */ }
            else {
                const int keep = prefix - bitStart;
                const std::uint8_t mask =
                    static_cast<std::uint8_t>(~((1u << (8 - keep)) - 1));
                a[i] = static_cast<std::uint8_t>(a[i] & mask);
            }
        }
        r.v6Addr = a;
        r.v6PrefixBits = prefix;
        return r;
    }
}

bool matchCidr(const CidrRange& range, const std::string& ip) {
    const bool ipIsV6 = ip.find(':') != std::string::npos;
    if (range.isV4 && ipIsV6) {
        // Cas spécial : IPv4-mapped IPv6 "::ffff:192.168.1.1"
        const auto pos = ip.rfind(':');
        if (pos != std::string::npos) {
            const auto v4 = ip.substr(pos + 1);
            if (v4.find('.') != std::string::npos) {
                std::uint32_t v = 0;
                if (parseIPv4Numeric(v4, v)) {
                    return (v & range.v4Mask) == range.v4Addr;
                }
            }
        }
        return false;
    }
    if (!range.isV4 && !ipIsV6) {
        return false;
    }
    if (range.isV4) {
        std::uint32_t v = 0;
        if (!parseIPv4Numeric(ip, v)) return false;
        return (v & range.v4Mask) == range.v4Addr;
    }
    // IPv6
    std::array<std::uint8_t,16> a{};
    if (!parseIPv6Numeric(ip, a)) return false;
    const int prefix = range.v6PrefixBits;
    for (int i = 0; i < 16; ++i) {
        const int bitStart = i * 8;
        if (bitStart >= prefix) break;
        if (bitStart + 8 <= prefix) {
            if (a[i] != range.v6Addr[i]) return false;
        } else {
            const int keep = prefix - bitStart;
            const std::uint8_t mask =
                static_cast<std::uint8_t>(~((1u << (8 - keep)) - 1));
            if ((a[i] & mask) != range.v6Addr[i]) return false;
        }
    }
    return true;
}

} // namespace ltr::infra
