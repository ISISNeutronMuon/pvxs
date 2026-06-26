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

#include "sitehooks.h"

namespace pvxs {
namespace ioc {
namespace site {

namespace {
// Function-local statics avoid init-order issues with site-specific registrars.
std::vector<std::function<void()>>& hooksAtBeginning() {
    static std::vector<std::function<void()>> v;
    return v;
}
std::vector<std::function<void()>>& hooksAfterIocBuilt() {
    static std::vector<std::function<void()>> v;
    return v;
}
std::function<const char*(epicsUInt16, dbCommon*, const Value&)>& alarmStringFn() {
    static std::function<const char*(epicsUInt16, dbCommon*, const Value&)> fn;
    return fn;
}
std::set<std::string>& filteredNames() {
    static std::set<std::string> s;
    return s;
}
} // namespace

void markFiltered(const char* name)
{
    filteredNames().insert(name);
}

bool isNameFiltered(const char* pvName)
{
    auto& names = filteredNames();
    return !names.empty() && names.count(std::string(pvName, strcspn(pvName, ".")));
}

void addInitHookAtBeginning(std::function<void()> fn)
{
    hooksAtBeginning().push_back(std::move(fn));
}

void addInitHookAfterIocBuilt(std::function<void()> fn)
{
    hooksAfterIocBuilt().push_back(std::move(fn));
}

void setAlarmString(std::function<const char*(epicsUInt16, dbCommon*, const Value&)> fn)
{
    alarmStringFn() = std::move(fn);
}

void fireHooksAtBeginning()
{
    for (auto& fn : hooksAtBeginning()) fn();
}

void fireHooksAfterIocBuilt()
{
    for (auto& fn : hooksAfterIocBuilt()) fn();
}

const char* alarmString(epicsUInt16 status, dbCommon* prec, const Value& node)
{
    auto& fn = alarmStringFn();
    return fn ? fn(status, prec, node) : nullptr;
}

} // site
} // ioc
} // pvxs
