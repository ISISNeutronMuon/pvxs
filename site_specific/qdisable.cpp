/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <cassert>
#include <cstring>

#include "dbentry.h"
#include "sitehooks.h"

namespace {

bool isQDisabled(dbCommon* prec)
{
    assert(prec);
    pvxs::ioc::DBEntry ent(prec);
    const char* val = ent.info("Q:DISABLE");
    return val && val[0] != '\0' && strcmp(val, "0") != 0;
}

struct Registrar {
    Registrar() { pvxs::ioc::site::setPVFilter(isQDisabled); }
} s_registrar;

} // namespace
