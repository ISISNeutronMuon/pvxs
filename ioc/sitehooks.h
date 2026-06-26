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

// Single predicate; returns true for records that should not be served as PVs.
void setPVFilter(std::function<bool(dbCommon*)> fn);

// Multiple callbacks may be registered; all are fired in registration order.
void addInitHookAtBeginning(std::function<void()> fn);
void addInitHookAfterIocBuilt(std::function<void()> fn);

// Single alarm-string provider; replaces any previously registered one.
void setAlarmStringFn(std::function<const char*(epicsUInt16, dbCommon*, const Value&)> fn);

// --- Dispatch (called by core ioc/ code) ---

// Returns true if the registered filter says the record should be excluded.
bool isPVFiltered(dbCommon* prec);

// Pre-computed name-based filter set (populated via markFiltered during iocInit).
// isNameFiltered accepts bare record names or "RECORD.FIELD" PV names.
void markFiltered(const char* name);
bool isNameFiltered(const char* pvName);

void initHookAtBeginning();
void initHookAfterIocBuilt();

// Returns nullptr when no provider is registered; caller should use the EPICS default.
const char* alarmString(epicsUInt16 status, dbCommon* prec, const Value& node);

} // site
} // ioc
} // pvxs

#endif // PVXS_SITEHOOKS_H
