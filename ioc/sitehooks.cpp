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
std::function<void(dbCommon*, Value&)>& nodePostProcessorFn() {
    static std::function<void(dbCommon*, Value&)> fn;
    return fn;
}
std::set<std::string>& filteredNames() {
    static std::set<std::string> s;
    return s;
}
void siteHookDispatch(initHookState state) noexcept
{
    if (state == initHookAtBeginning)
        for (auto& fn : hooksAtBeginning()) fn();
    else if (state == initHookAfterIocBuilt)
        for (auto& fn : hooksAfterIocBuilt()) fn();
}
struct SiteHooksRegistrar {
    SiteHooksRegistrar() { initHookRegister(siteHookDispatch); }
} s_registrar;
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

void setNodePostProcessor(std::function<void(dbCommon*, Value&)> fn)
{
    nodePostProcessorFn() = std::move(fn);
}

void postProcessNode(dbCommon* prec, Value& node)
{
    auto& fn = nodePostProcessorFn();
    if (fn) fn(prec, node);
}

} // site
} // ioc
} // pvxs
