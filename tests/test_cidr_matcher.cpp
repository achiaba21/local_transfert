// Tests CidrMatcher : parsing IPv4/IPv6 + matching.

#include <cassert>
#include <cstdio>

#include "ltr/infra/cidr_matcher.hpp"

int main() {
    using namespace ltr::infra;

    // IPv4 CIDR /24.
    {
        const auto r = parseCidr("192.168.1.0/24");
        assert(r);
        assert(r->isV4);
        assert(matchCidr(*r, "192.168.1.5"));
        assert(matchCidr(*r, "192.168.1.255"));
        assert(!matchCidr(*r, "192.168.2.1"));
        assert(!matchCidr(*r, "10.0.0.1"));
    }

    // IPv4 single host = /32 implicite.
    {
        const auto r = parseCidr("10.0.0.42");
        assert(r);
        assert(r->isV4);
        assert(matchCidr(*r, "10.0.0.42"));
        assert(!matchCidr(*r, "10.0.0.43"));
    }

    // IPv4 CIDR /8.
    {
        const auto r = parseCidr("10.0.0.0/8");
        assert(r);
        assert(matchCidr(*r, "10.0.0.1"));
        assert(matchCidr(*r, "10.255.255.255"));
        assert(!matchCidr(*r, "11.0.0.1"));
    }

    // CIDR /0 = tout matché.
    {
        const auto r = parseCidr("0.0.0.0/0");
        assert(r);
        assert(matchCidr(*r, "1.2.3.4"));
        assert(matchCidr(*r, "8.8.8.8"));
    }

    // IPv6 loopback.
    {
        const auto r = parseCidr("::1/128");
        assert(r);
        assert(!r->isV4);
        assert(matchCidr(*r, "::1"));
        assert(!matchCidr(*r, "::2"));
    }

    // IPv6 prefix /64.
    {
        const auto r = parseCidr("fe80::/64");
        assert(r);
        assert(matchCidr(*r, "fe80::1"));
        assert(matchCidr(*r, "fe80::abcd:1234"));
        assert(!matchCidr(*r, "fd00::1"));
    }

    // IPv6 single sans CIDR = /128 implicite.
    {
        const auto r = parseCidr("::1");
        assert(r);
        assert(matchCidr(*r, "::1"));
        assert(!matchCidr(*r, "::2"));
    }

    // CIDR invalides.
    {
        assert(!parseCidr(""));
        assert(!parseCidr("abc"));
        assert(!parseCidr("192.168.1.256"));
        assert(!parseCidr("192.168.1.0/33"));
        assert(!parseCidr("::1/129"));
        assert(!parseCidr("192.168.1.0/-1"));
    }

    // IPv4 CIDR ne matche pas une IPv6 (sauf cas IPv4-mapped).
    {
        const auto r = parseCidr("192.168.1.0/24");
        assert(r);
        assert(!matchCidr(*r, "fe80::1"));
        // IPv4-mapped IPv6 : "::ffff:192.168.1.5" doit matcher.
        assert(matchCidr(*r, "::ffff:192.168.1.5"));
    }

    std::printf("test_cidr_matcher OK\n");
    return 0;
}
