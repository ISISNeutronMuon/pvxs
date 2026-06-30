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

void populateFilteredNames()
{
    pvxs::ioc::DBEntry dbEntry;
    for (long status = dbFirstRecordType(dbEntry); !status; status = dbNextRecordType(dbEntry)) {
        for (status = dbFirstRecord(dbEntry); !status; status = dbNextRecord(dbEntry)) {
            if (isQDisabled(static_cast<dbCommon*>(dbEntry->precnode->precord)))
                pvxs::ioc::site::markFiltered(dbEntry->precnode->recordname);
        }
    }
}

} // namespace

namespace pvxs { namespace ioc { namespace site {
void registerQdisable() {
    addInitHookAtBeginning(populateFilteredNames);
}
}}} // pvxs::ioc::site
