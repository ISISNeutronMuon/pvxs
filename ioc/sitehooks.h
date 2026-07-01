/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_SITEHOOKS_H
#define PVXS_SITEHOOKS_H

#include <functional>
#include <epicsTypes.h>
#include <dbCommon.h>

#include <pvxs/data.h>
#include <pvxs/iochooks.h>

namespace pvxs {
namespace ioc {
namespace site {

// --- Registration (called by site-specific code) ---

// Multiple callbacks may be registered; all are fired in registration order.
PVXS_IOC_API void addInitHookAtBeginning(std::function<void()> fn);
PVXS_IOC_API void addInitHookAfterIocBuilt(std::function<void()> fn);

// Multiple callbacks may be registered; all are fired in registration order.
// Called at the end of IOCSource::get() with the record locked; may modify any field in node.
PVXS_IOC_API void addNodePostProcessor(std::function<void(dbCommon*, Value&)> fn);

// Register a channel filter.  fn(pvName, peerAddr) returns true to allow, false to deny.
// pvName is the bare record name.  peerAddr is "X.X.X.X:port" (IPv4) or "[addr]:port" (IPv6),
// or nullptr when called during name-list construction (no peer context).
// Filters that deny unconditionally should ignore peerAddr.
// Filters that restrict by address must return true when peerAddr is nullptr
// so that the record remains in the name list.
// Multiple callbacks may be registered; all must return true for the channel to be allowed.
PVXS_IOC_API void addChannelFilter(std::function<bool(const char* pvName, const char* peerAddr)> fn);

// --- Called once from pvxsBaseRegistrar ---
void registerHooks();

// Defined in the generated siteregister.cpp; calls every registerXxx()
// function discovered by site/gen_siteregister.py at build time.
void registerSiteExtensions();

// --- Dispatch (called by core ioc/ code) ---

// Returns true if all registered channel filters allow this (pvName, peerAddr) pair.
// pvName is a bare record name; peerAddr is in the same format as addChannelFilter,
// or nullptr when called during name-list construction.
PVXS_IOC_API bool isChannelAllowed(const char* pvName, const char* peerAddr);

void postProcessNode(dbCommon* prec, Value& node);

} // site
} // ioc
} // pvxs

#endif // PVXS_SITEHOOKS_H
