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

// Multiple callbacks may be registered; all are fired in registration order.
void addInitHookAtBeginning(std::function<void()> fn);
void addInitHookAfterIocBuilt(std::function<void()> fn);

// Single alarm-string provider; replaces any previously registered one.
void setAlarmStringFn(std::function<const char*(epicsUInt16, dbCommon*, const Value&)> fn);

// --- Dispatch (called by iochooks.cpp and iocsource.cpp) ---

void initHookAtBeginning();
void initHookAfterIocBuilt();

// Returns nullptr when no provider is registered; caller should use the EPICS default.
const char* alarmString(epicsUInt16 status, dbCommon* prec, const Value& node);

} // site
} // ioc
} // pvxs

#endif // PVXS_SITEHOOKS_H
