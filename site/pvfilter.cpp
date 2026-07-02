/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

/*
 * pvfilter.cpp -- site extension: PV channel filters
 *
 * Provides two info-field-driven filters for PVA channel access:
 *
 *   Q:pv:disable      -- suppress a record from PVA entirely (name list,
 *                        search, and channel open).  Set to any non-zero,
 *                        non-empty value to enable suppression.
 *
 *   Q:pv:loopback_only -- restrict a record to clients connecting via the
 *                        loopback interface (127.0.0.0/8 or ::1).  The
 *                        record remains visible in the PV name list so
 *                        that local tools can discover it.
 *
 * Both filters are evaluated once at initHookAtBeginning and stored in
 * in-memory sets for zero-overhead per-request lookup.
 */

#include <cstring>
#include <unordered_set>
#include <string>

// Use platform socket headers and inet_pton() directly rather than pvxs::SockAddr
// (src/osiSockExt.h). SockAddr's parser falls back to a synchronous DNS lookup
// for any string that isn't a literal numeric address, and isLoopbackAddr() runs
// on the search-reply hot path (once per candidate PV name per PVA search) -- a
// malformed or unexpected peerAddr there must fail fast, not risk blocking on the
// network. inet_pton() never resolves hostnames, so it can't hit that path.
#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#endif

#include "dbentry.h"
#include "sitehooks.h"

namespace {

// Function-local statics (Meyers singletons) avoid namespace-scope static
// constructors, which are flagged by the EPICS CDT check.

// --- Q:pv:disable ---

std::unordered_set<std::string>& disabledSet()
{
    static std::unordered_set<std::string> s;
    return s;
}

// --- Q:pv:loopback_only ---

std::unordered_set<std::string>& loopbackOnlySet()
{
    static std::unordered_set<std::string> s;
    return s;
}

void populateSets()
{
    auto& disabled = disabledSet();
    auto& loopback = loopbackOnlySet();
    disabled.clear();
    loopback.clear();
    pvxs::ioc::forEachRecord([&](dbCommon* prec) {
        pvxs::ioc::DBEntry infoEnt(prec);
        if (pvxs::ioc::site::infoFlagSet(infoEnt.info("Q:pv:disable")))
            disabled.insert(prec->name);
        if (pvxs::ioc::site::infoFlagSet(infoEnt.info("Q:pv:loopback_only")))
            loopback.insert(prec->name);
    });
}

// Returns true if peerAddr ("X.X.X.X:port" or "[addr]:port") is a loopback address.
// Covers the full IPv4 loopback range 127.0.0.0/8 and the IPv6 loopback address ::1.
static bool isLoopbackAddr(const char* peerAddr)
{
    if (!peerAddr || !peerAddr[0])
        return false;

    if (peerAddr[0] == '[') {
        // IPv6: "[addr]:port" -- extract the address between the brackets
        const char* end = strchr(peerAddr + 1, ']');
        if (!end)
            return false;
        std::string addr(peerAddr + 1, end);
        struct in6_addr a6;
        if (inet_pton(AF_INET6, addr.c_str(), &a6) != 1)
            return false;
        return IN6_IS_ADDR_LOOPBACK(&a6);
    } else {
        // IPv4: "X.X.X.X:port" -- strip the trailing ":port"
        const char* colon = strrchr(peerAddr, ':');
        std::string addr(peerAddr, colon ? (size_t)(colon - peerAddr) : strlen(peerAddr));
        struct in_addr a4;
        if (inet_pton(AF_INET, addr.c_str(), &a4) != 1)
            return false;
        // 127.0.0.0/8 is the full loopback range
        return (ntohl(a4.s_addr) >> 24) == 127u;
    }
}

} // namespace

namespace pvxs { namespace ioc { namespace site {
void registerPvfilter() {
    addInitHookAtBeginning(populateSets);
    // Q:pv:disable: unconditionally deny including name-list (peerAddr ignored)
    addChannelFilter([](const char* pvName, const char* /*peerAddr*/) {
        return !disabledSet().count(pvName);
    });
    // Q:pv:loopback_only: deny non-loopback peers; return true for nullptr (name-list)
    addChannelFilter([](const char* pvName, const char* peerAddr) {
        if (!loopbackOnlySet().count(pvName)) return true;
        if (!peerAddr) return true;
        return isLoopbackAddr(peerAddr);
    });
}
}}} // pvxs::ioc::site
