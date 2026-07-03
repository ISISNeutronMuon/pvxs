/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_SITEHOOKS_H
#define PVXS_SITEHOOKS_H

#include <cstring>
#include <functional>
#include <epicsTypes.h>
#include <dbCommon.h>

#include <pvxs/data.h>
#include <pvxs/iochooks.h>

namespace pvxs {
namespace ioc {
namespace site {

// --- Utility (for use by site-specific code) ---

// Returns true if the value of a boolean-style "Q:*" info field represents
// "set": present, non-empty, and not the literal string "0".
inline bool infoFlagSet(const char* val) {
    return val && val[0] != '\0' && std::strcmp(val, "0") != 0;
}

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

// Returns true if pvName has Q:pv:disable / Q:pv:loopback_only set, respectively.
// Used by GroupSource (groupsource.cpp) both to enforce these per-field within
// a group, and to warn at group-processing time when such a record is added.
bool isPvDisabled(const char* pvName);
bool isPvLoopbackOnly(const char* pvName);

} // site
} // ioc
} // pvxs

#endif // PVXS_SITEHOOKS_H
