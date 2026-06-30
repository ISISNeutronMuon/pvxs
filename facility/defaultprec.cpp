/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include "facilityhooks.h"

namespace {

// Set display.precision to 2 when the record's PREC field is 0 (the default).
// Prevents clients from displaying floating-point values with zero decimal places
// simply because the database author omitted an explicit PREC setting.
void applyDefaultPrecision(dbCommon* /*prec*/, pvxs::Value& node)
{
    if (auto fld = node["display.precision"]) {
        if (fld.as<int32_t>() == 0)
            fld = int32_t(2);
    }
}

} // namespace

namespace pvxs { namespace ioc { namespace facility {
void registerDefaultprec() {
    addNodePostProcessor(applyDefaultPrecision);
}
}}} // pvxs::ioc::facility
