/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

/*
 * pvfilter.cpp -- site extension: PV channel filters
 *
 * A single info field, Q:pva:access, selects one of three mutually exclusive
 * access policies for a record's PVA channel. Being one string-valued field
 * rather than two independent booleans, a record can never simultaneously
 * request both "disable" and "loopback_only" -- there is nowhere to write
 * two conflicting values at once.
 *
 *   Q:pva:access = "enable"        -- normal access, no restriction. Same
 *                                    as the field being absent entirely;
 *                                    only useful to be explicit in a .db.
 *
 *   Q:pva:access = "disable"       -- suppress the record from PVA entirely
 *                                    (name list, search, and channel open).
 *
 *   Q:pva:access = "loopback_only" -- restrict the record to clients
 *                                    connecting via the loopback interface
 *                                    (127.0.0.0/8 or ::1). The record
 *                                    remains visible in the PV name list so
 *                                    that local tools can discover it.
 *
 * Any other value (a typo, most likely) is treated the same as "enable" --
 * i.e. it fails open to no restriction, not the more restrictive "disable"
 * -- but is logged as a startup warning so the .db author notices.
 *
 * All records are evaluated once at initHookAtBeginning and stored in an
 * in-memory map for zero-overhead per-request lookup.
 *
 * Scope: enforcement lives here (isChannelAllowed()) for SingleSource's own
 * direct single-PV channel. Group PVs (GroupSource) reuse isChannelAllowed()
 * itself, per-field, at ioc/groupsource.cpp's onGet/onPutGroup/onSubscribe
 * (via a local fieldAccessAllowed() helper there) -- it is not a separate
 * enforcement path. A flagged field is left out of the GET response entirely
 * (not sent as a zeroed/blank value), rejects PUT, and is excluded from
 * monitor updates within an otherwise-still-served group; the
 * rest of the group and the group PV itself are unaffected.
 *
 * ServerListFilterSource below applies the same isChannelAllowed() decision
 * to a third path: the "server" PV's "channels" RPC (what pvxlist <ip:port>
 * enumerates), which core pvxs's Source::onList() has no peer address to
 * filter by. makeServerListFilterSource() is installed by
 * ioc/sitehooks.cpp's registerHooks() in place of the core-registered
 * "server" source; it isn't pvfilter-specific (it enforces whatever the
 * combined isChannelAllowed() chain says, not just this file's own filter),
 * but lives here because this is where the record-name policy it reads is
 * defined and because touching core pvxs (src/) is avoided.
 *
 * isPvDisabled()/isPvLoopbackOnly() below expose the individual flags
 * (rather than a single allow/deny decision) purely so
 * GroupSource::GroupSource() (groupsource.cpp) can print a specific startup
 * warning when a flagged record is added to a group, so the .db author
 * notices even before any client touches that field. They play no part in
 * enforcement.
 */

#include <cstring>
#include <memory>
#include <set>
#include <unordered_map>
#include <string>

#include <pvxs/nt.h>
#include <pvxs/source.h>
#include <pvxs/version.h>

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

// include last to avoid clash of #define printf with other headers
#include <epicsStdio.h>

namespace {

// Function-local static variable (Meyers singleton) avoids a namespace-scope
// static constructor, which is flagged by the EPICS CDT check.

enum class Access : uint8_t { Enabled, Disabled, LoopbackOnly };

// Per-PV access policy, keyed by record name so the hot path (isChannelAllowed)
// hashes pvName once rather than once per filter. Records not present here are
// implicitly Access::Enabled -- the common case is left out to save memory.
std::unordered_map<std::string, Access>& pvAccess()
{
    static std::unordered_map<std::string, Access> m;
    return m;
}

void populateSets()
{
    auto& access = pvAccess();
    access.clear();
    pvxs::ioc::forEachRecord([&](dbCommon* prec) {
        pvxs::ioc::DBEntry infoEnt(prec);
        const char* val = infoEnt.info("Q:pva:access");
        if (!val || strcmp(val, "enable") == 0)
            return;
        if (strcmp(val, "disable") == 0) {
            access[prec->name] = Access::Disabled;
        } else if (strcmp(val, "loopback_only") == 0) {
            access[prec->name] = Access::LoopbackOnly;
        } else {
            fprintf(stderr, "%s Warning: Q:pva:access has unrecognized value \"%s\"; "
                            "treating as \"enable\" (unrestricted)\n", prec->name, val);
        }
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
        if (IN6_IS_ADDR_V4MAPPED(&a6)) {
            // A dual-stack listening socket (bound to "::", which is what the
            // server falls back to whenever its usual port is already taken)
            // reports an IPv4 peer as an IPv4-mapped IPv6 address, "::ffff:
            // a.b.c.d" -- IN6_IS_ADDR_LOOPBACK only matches the literal ::1,
            // so without this a loopback client on such a server would be
            // misclassified as remote. The mapped IPv4 address is the last
            // 4 bytes of s6_addr.
            return a6.s6_addr[12] == 127u;
        }
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

// Re-implements the built-in "server" PV (normally src/serversource.cpp, core
// pvxs) so its "channels" RPC -- what pvxlist <ip:port> drives -- can apply
// the same Q:pva:access filtering as onSearch()/onCreate(), using the real
// requesting peer's address. core pvxs's Source::onList() (include/pvxs/
// source.h) has no peer argument, so core's own "channels" handler can't do
// this per-source; replacing the whole source here, entirely within the
// ioc/ site-extension layer and its public pvxs::server::Server API
// (removeSource()/addSource()/listSource()/getSource()), avoids having to
// change core pvxs at all.
//
// Installed by ioc/sitehooks.cpp's registerHooks() in place of the
// core-registered "__server" source, at the same (name, order) registry
// slot, so pvxlist/pvinfo see no difference except that disabled/
// loopback_only names are now correctly excluded from a non-loopback
// requester's enumeration.
struct ServerListFilterSource : public pvxs::server::Source
{
    const pvxs::Value info = pvxs::TypeDef(pvxs::TypeCode::Struct, {
                                    pvxs::Member(pvxs::TypeCode::String, "implLang"),
                                    pvxs::Member(pvxs::TypeCode::String, "version"),
                                }).create();

    void onSearch(Search&) override {
        // "server" is not advertised via search, matching core's ServerSource.
    }

    void onCreate(std::unique_ptr<pvxs::server::ChannelControl>&& op) override {
        if (op->name() != "server")
            return;

        op->onRPC([this](std::unique_ptr<pvxs::server::ExecOp>&& eop, pvxs::Value&& raw) {
            auto args = std::move(raw);

            if (auto Q = args["query"]) // NTURI
                args = Q;

            if (args["help"].valid()) {
                auto ret = pvxs::nt::NTScalar{pvxs::TypeCode::String}.create();
                ret["value"] = "Help, I really should write some help";
                eop->reply(ret);
                return;
            }

            auto reqOp = args["op"].as<std::string>();

            if (reqOp == "channels") {
                std::set<std::string> names;
                {
                    auto srv = pvxs::ioc::server();
                    for (auto& pair : srv.listSource()) {
                        auto src = srv.getSource(pair.first, pair.second);
                        if (!src)
                            continue;
                        auto list = src->onList();
                        if (list.names) {
                            for (auto& nm : *list.names)
                                names.insert(nm);
                        }
                    }
                }

                auto& peerName = eop->peerName();
                for (auto it = names.begin(); it != names.end(); ) {
                    if (pvxs::ioc::site::isChannelAllowed(it->c_str(), peerName.c_str()))
                        ++it;
                    else
                        it = names.erase(it);
                }

                pvxs::shared_array<std::string> lnames(names.begin(), names.end());

                auto ret = pvxs::nt::NTScalar{pvxs::TypeCode::StringA}.create();
                ret["value"] = lnames.freeze().castTo<const void>();
                eop->reply(ret);
                return;

            } else if (reqOp == "info") {
                auto ret = info.cloneEmpty();
                ret["implLang"] = "cpp";
                ret["version"] = pvxs::version_str();
                eop->reply(ret);
                return;
            }

            eop->error("Not implemented");
        });
    }
};

} // namespace

namespace pvxs { namespace ioc { namespace site {
bool isPvDisabled(const char* pvName) {
    auto& access = pvAccess();
    auto it = access.find(pvName);
    return it != access.end() && it->second == Access::Disabled;
}

bool isPvLoopbackOnly(const char* pvName) {
    auto& access = pvAccess();
    auto it = access.find(pvName);
    return it != access.end() && it->second == Access::LoopbackOnly;
}

void registerPvfilter() {
    addInitHookAtBeginning(populateSets);
    addChannelFilter([](const char* pvName, const char* peerAddr) {
        auto& access = pvAccess();
        auto it = access.find(pvName);
        if (it == access.end())
            return true;
        // "disable": unconditionally deny, including name-list (peerAddr ignored)
        if (it->second == Access::Disabled)
            return false;
        // "loopback_only": deny non-loopback peers; allow for nullptr (name-list)
        if (it->second == Access::LoopbackOnly && peerAddr && !isLoopbackAddr(peerAddr))
            return false;
        return true;
    });
    setServerSourceFactory([]() -> std::shared_ptr<server::Source> {
        return std::make_shared<ServerListFilterSource>();
    });
}
}}} // pvxs::ioc::site
