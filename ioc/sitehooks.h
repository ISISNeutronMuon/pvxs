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

namespace pvxs {
namespace ioc {
namespace site {

// --- Registration (called by site-specific code) ---

// Pre-populate the name-based filter set (accepts bare record names).
void markFiltered(const char* name);

// Multiple callbacks may be registered; all are fired in registration order.
void addInitHookAtBeginning(std::function<void()> fn);
void addInitHookAfterIocBuilt(std::function<void()> fn);

// Single alarm-string provider; replaces any previously registered one.
void setAlarmString(std::function<const char*(epicsUInt16, dbCommon*, const Value&)> fn);

// --- Dispatch (called by core ioc/ code) ---

// Returns true if the record name was pre-registered via markFiltered.
// Accepts bare record names or "RECORD.FIELD" PV names.
bool isNameFiltered(const char* pvName);

void fireHooksAtBeginning();
void fireHooksAfterIocBuilt();

// Returns nullptr when no provider is registered; caller should use the EPICS default.
const char* alarmString(epicsUInt16 status, dbCommon* prec, const Value& node);

} // site
} // ioc
} // pvxs

#endif // PVXS_SITEHOOKS_H
