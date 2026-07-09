/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_SITEHOOKS_H
#define PVXS_SITEHOOKS_H

#include <functional>
#include <memory>
#include <dbCommon.h>

#include <pvxs/data.h>
#include <pvxs/iochooks.h>
#include <pvxs/server.h>

namespace pvxs {
namespace ioc {
namespace site {

// --- Registration (called by site-specific code) ---

// Multiple callbacks may be registered; all are fired in registration order.
// Only called from within pvxsIoc (alarmmsg.cpp, pvfilter.cpp), so no PVXS_IOC_API
// is needed -- unlike isChannelAllowed() below, these never cross a DLL boundary.
void addInitHookAtBeginning(std::function<void()> fn);
void addInitHookAfterIocBuilt(std::function<void()> fn);

// Multiple callbacks may be registered; all are fired in registration order.
// Called at the end of IOCSource::get() with the record locked; may modify any field in node.
void addNodePostProcessor(std::function<void(dbCommon*, Value&)> fn);

// Register a channel filter.  fn(pvName, peerAddr) returns true to allow, false to deny.
// pvName is the bare record name.  peerAddr is "X.X.X.X:port" (IPv4) or "[addr]:port" (IPv6),
// or nullptr when called during name-list construction (no peer context).
// Filters that deny unconditionally should ignore peerAddr.
// Filters that restrict by address must return true when peerAddr is nullptr
// so that the record remains in the name list.
// Multiple callbacks may be registered; all must return true for the channel to be allowed.
void addChannelFilter(std::function<bool(const char* pvName, const char* peerAddr)> fn);

// Register the factory used to build a replacement for the core-registered
// "server" PV source (see registerHooks() in sitehooks.cpp for why one is
// needed). Unlike the addXxx() registrations above, there is only one
// "server" source slot to fill, so this is a singleton setter, not a list:
// a later call replaces any previously registered factory. If none is ever
// registered, the core-provided "server" source is left in place unchanged.
void setServerSourceFactory(std::function<std::shared_ptr<server::Source>()> fn);

// --- Called once from pvxsBaseRegistrar ---
void registerHooks();

// --- Dispatch (called by core ioc/ code) ---

// Returns true if all registered channel filters allow this (pvName, peerAddr) pair.
// pvName is a bare record name; peerAddr is in the same format as addChannelFilter,
// or nullptr when called during name-list construction.
// PVXS_IOC_API is required here: test/testpvfilter.cpp calls this directly from a
// separate binary linked against pvxsIoc, so it does cross a DLL boundary on Windows.
PVXS_IOC_API bool isChannelAllowed(const char* pvName, const char* peerAddr);

void postProcessNode(dbCommon* prec, Value& node);

// Returns true if pvName's Q:pva:access is "disable" / "loopback_only", respectively.
// Used only by GroupSource (groupsource.cpp) to print a specific startup warning
// when such a record is added to a group -- per-field enforcement within a group
// goes through isChannelAllowed() above instead, like SingleSource's does.
bool isPvDisabled(const char* pvName);
bool isPvLoopbackOnly(const char* pvName);

} // site
} // ioc
} // pvxs

#endif // PVXS_SITEHOOKS_H
