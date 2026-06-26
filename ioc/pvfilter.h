/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PVXS_PVFILTER_H
#define PVXS_PVFILTER_H

#include <functional>
#include <dbCommon.h>

namespace pvxs {
namespace ioc {

// Register a predicate that returns true for records that should not be served as PVs.
// Called by site-specific code at static-initialisation time before IOC start-up.
void setPVFilter(std::function<bool(dbCommon*)> fn);

// Returns true if the registered filter says the record should be excluded.
// Returns false when no filter has been registered.
bool isPVFiltered(dbCommon* prec);

} // ioc
} // pvxs

#endif // PVXS_PVFILTER_H
