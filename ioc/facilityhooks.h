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

namespace pvxs {
namespace ioc {
namespace facility {

// --- Registration (called by facility-specific code) ---

// Multiple callbacks may be registered; all are fired in registration order.
void addInitHookAtBeginning(std::function<void()> fn);
void addInitHookAfterIocBuilt(std::function<void()> fn);

// Single node post-processor; replaces any previously registered one.
// Called at the end of IOCSource::get() with the record locked; may modify any field in node.
void setNodePostProcessor(std::function<void(dbCommon*, Value&)> fn);

// --- Dispatch (called by core ioc/ code) ---

void postProcessNode(dbCommon* prec, Value& node);

} // facility
} // ioc
} // pvxs

#endif // PVXS_FACILITYHOOKS_H
