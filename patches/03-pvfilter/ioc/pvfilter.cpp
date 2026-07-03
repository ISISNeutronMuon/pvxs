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
 * Both filters are evaluated once at initHookAtBeginning and stored in an
 * in-memory map for zero-overhead per-request lookup.
 *
 * Scope: enforcement lives here (isChannelAllowed()) for SingleSource's own
 * direct single-PV channel. Group PVs (GroupSource) reuse isChannelAllowed()
 * itself, per-field, at ioc/groupsource.cpp's onGet/onPutGroup/onSubscribe
 * (via a local fieldAccessAllowed() helper there) -- it is not a separate
 * enforcement path. A flagged field is blanked on GET, rejects PUT, and is
 * excluded from monitor updates within an otherwise-still-served group; the
 * rest of the group and the group PV itself are unaffected.
 *
 * isPvDisabled()/isPvLoopbackOnly() below expose the individual flags
 * (rather than a single allow/deny decision) purely so
 * GroupSource::GroupSource() (groupsource.cpp) can print a specific startup
 * warning when a flagged record is added to a group, so the .db author
 * notices even before any client touches that field. They play no part in
 * enforcement.
 */

#include <cstring>
#include <unordered_map>
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
#  include <sys/socket.h>
#  include <arpa/inet.h>
#  include <netinet/in.h>
#endif

#include "dbentry.h"
#include "sitehooks.h"

namespace {

// Function-local static variable (Meyers singleton) avoids a namespace-scope
// static constructor, which is flagged by the EPICS CDT check.

constexpr uint8_t kDisabled = 1u << 0;
constexpr uint8_t kLoopbackOnly = 1u << 1;

// Per-PV filter flags, keyed by record name so the hot path (isChannelAllowed)
// hashes pvName once rather than once per filter.
std::unordered_map<std::string, uint8_t>& pvFlags()
{
    static std::unordered_map<std::string, uint8_t> m;
    return m;
}

void populateSets()
{
    auto& flags = pvFlags();
    flags.clear();
    pvxs::ioc::forEachRecord([&](dbCommon* prec) {
        pvxs::ioc::DBEntry infoEnt(prec);
        uint8_t f = 0;
        if (pvxs::ioc::site::infoFlagSet(infoEnt.info("Q:pv:disable")))
            f |= kDisabled;
        if (pvxs::ioc::site::infoFlagSet(infoEnt.info("Q:pv:loopback_only")))
            f |= kLoopbackOnly;
        if (f)
            flags[prec->name] = f;
    });
}

// Returns true if peerAddr ("X.X.X.X:port" or "[addr]:port") is a loopback address.
// Covers the full IPv4 loopback range 127.0.0.0/8 and the IPv6 loopback address ::1.
bool isLoopbackAddr(const char* peerAddr)
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
bool isPvDisabled(const char* pvName) {
    auto& flags = pvFlags();
    auto it = flags.find(pvName);
    return it != flags.end() && (it->second & kDisabled);
}

bool isPvLoopbackOnly(const char* pvName) {
    auto& flags = pvFlags();
    auto it = flags.find(pvName);
    return it != flags.end() && (it->second & kLoopbackOnly);
}

void registerPvfilter() {
    addInitHookAtBeginning(populateSets);
    addChannelFilter([](const char* pvName, const char* peerAddr) {
        auto& flags = pvFlags();
        auto it = flags.find(pvName);
        if (it == flags.end())
            return true;
        // Q:pv:disable: unconditionally deny, including name-list (peerAddr ignored)
        if (it->second & kDisabled)
            return false;
        // Q:pv:loopback_only: deny non-loopback peers; allow for nullptr (name-list)
        if ((it->second & kLoopbackOnly) && peerAddr && !isLoopbackAddr(peerAddr))
            return false;
        return true;
    });
}
}}} // pvxs::ioc::site
