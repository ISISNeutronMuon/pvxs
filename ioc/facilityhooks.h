/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_FACILITYHOOKS_H
#define PVXS_FACILITYHOOKS_H

#include <functional>
#include <epicsTypes.h>
#include <dbCommon.h>

#include <pvxs/data.h>
#include <pvxs/iochooks.h>

namespace pvxs {
namespace ioc {
namespace facility {

// --- Registration (called by facility-specific code) ---

// Multiple callbacks may be registered; all are fired in registration order.
PVXS_IOC_API void addInitHookAtBeginning(std::function<void()> fn);
PVXS_IOC_API void addInitHookAfterIocBuilt(std::function<void()> fn);

// Single node post-processor; replaces any previously registered one.
// Called at the end of IOCSource::get() with the record locked; may modify any field in node.
PVXS_IOC_API void setNodePostProcessor(std::function<void(dbCommon*, Value&)> fn);

// --- Called once from pvxsBaseRegistrar ---
void registerHooks();

// Defined in the generated facilityregister.cpp; calls every registerXxx()
// function discovered by facility/gen_facilityregister.py at build time.
void registerFacilities();

// --- Dispatch (called by core ioc/ code) ---

void postProcessNode(dbCommon* prec, Value& node);

} // facility
} // ioc
} // pvxs

#endif // PVXS_FACILITYHOOKS_H
