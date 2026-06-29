/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <cstring>
#include <set>
#include <string>
#include <vector>
#include <functional>

#include <initHooks.h>

#include "facilityhooks.h"

namespace pvxs {
namespace ioc {
namespace facility {

namespace {
// Function-local statics avoid init-order issues with facility-specific registrars.
std::vector<std::function<void()>>& hooksAtBeginning() {
    static std::vector<std::function<void()>> v;
    return v;
}
std::vector<std::function<void()>>& hooksAfterIocBuilt() {
    static std::vector<std::function<void()>> v;
    return v;
}
std::function<void(dbCommon*, Value&)>& nodePostProcessorFn() {
    static std::function<void(dbCommon*, Value&)> fn;
    return fn;
}
std::set<std::string>& filteredNames() {
    static std::set<std::string> s;
    return s;
}
void facilityHookDispatch(initHookState state) noexcept
{
    if (state == initHookAtBeginning)
        for (auto& fn : hooksAtBeginning()) fn();
    else if (state == initHookAfterIocBuilt)
        for (auto& fn : hooksAfterIocBuilt()) fn();
}
struct FacilityHooksRegistrar {
    FacilityHooksRegistrar() { initHookRegister(facilityHookDispatch); }
} s_registrar;
} // namespace

/**
 * Add a record name to the set of filtered (suppressed) PV names.
 * Should be called during an addInitHookAtBeginning callback before SingleSource is constructed.
 *
 * @param name bare record name (not a dotted field name)
 */
void markFiltered(const char* name)
{
    filteredNames().insert(name);
}

/**
 * Returns true if the given PV name has been suppressed via markFiltered.
 * Accepts bare record names ("RECORD") or dotted field names ("RECORD.FIELD");
 * the field suffix is stripped before the lookup.
 *
 * @param pvName PV name to test
 */
bool isNameFiltered(const char* pvName)
{
    auto& names = filteredNames();
    return !names.empty() && names.count(std::string(pvName, strcspn(pvName, ".")));
}

/**
 * Register a callback to be invoked at EPICS initHookAtBeginning.
 * Multiple callbacks may be registered; they are fired in registration order.
 * Use this phase to iterate the loaded database and pre-compute per-record data
 * (e.g. calling markFiltered) before SingleSource is constructed.
 *
 * @param fn callback to invoke
 */
void addInitHookAtBeginning(std::function<void()> fn)
{
    hooksAtBeginning().push_back(std::move(fn));
}

/**
 * Register a callback to be invoked at EPICS initHookAfterIocBuilt.
 * Multiple callbacks may be registered; they are fired in registration order.
 *
 * @param fn callback to invoke
 */
void addInitHookAfterIocBuilt(std::function<void()> fn)
{
    hooksAfterIocBuilt().push_back(std::move(fn));
}

/**
 * Register a node post-processor, called at the end of every IOCSource::get()
 * after all standard fields have been populated.
 * Only one post-processor may be registered; a subsequent call replaces the previous one.
 * The record is locked for the duration of the call.
 *
 * @param fn callback receiving the record pointer and the mutable PVA value node
 */
void setNodePostProcessor(std::function<void(dbCommon*, Value&)> fn)
{
    nodePostProcessorFn() = std::move(fn);
}

/**
 * Invoke the registered node post-processor, if any.
 * Called by IOCSource::get() after all standard fields have been written to node.
 *
 * @param prec pointer to the EPICS record (locked by the caller)
 * @param node the PVA value node to be returned to the client
 */
void postProcessNode(dbCommon* prec, Value& node)
{
    auto& fn = nodePostProcessorFn();
    if (fn) fn(prec, node);
}

} // facility
} // ioc
} // pvxs
