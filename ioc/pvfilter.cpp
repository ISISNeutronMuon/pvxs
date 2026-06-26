/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <functional>
#include <dbCommon.h>

#include "pvfilter.h"

namespace pvxs {
namespace ioc {

namespace {
// Function-local static avoids static-initialisation-order issues when
// site-specific code registers a filter from its own static initialiser.
std::function<bool(dbCommon*)>& filterFn() {
    static std::function<bool(dbCommon*)> fn;
    return fn;
}
} // namespace

void setPVFilter(std::function<bool(dbCommon*)> fn)
{
    filterFn() = std::move(fn);
}

bool isPVFiltered(dbCommon* prec)
{
    auto& fn = filterFn();
    return fn && fn(prec);
}

} // ioc
} // pvxs
