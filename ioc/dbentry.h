/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#ifndef PVXS_DBENTRY_H
#define PVXS_DBENTRY_H

#include <dbStaticLib.h>
#include <dbAccess.h>

#include <pvxs/iochooks.h>

namespace pvxs {
namespace ioc {

/**
 * Wrapper class for DBENTRY that is a type that encapsulates an IOC database entry.
 */
class DBEntry {
    DBENTRY ent{};
public:
    DBEntry() {
        dbInitEntry(pdbbase, &ent);
    }
    explicit DBEntry(dbCommon *prec) {
#if EPICS_VERSION_INT >= VERSION_INT(3, 16, 1, 0)
        dbInitEntryFromRecord(prec, &ent);
#else
        dbInitEntry(pdbbase, &ent);
        (void)dbFindRecord(&ent, prec->name);
#endif
    }
    DBEntry(const DBEntry&) = delete;
    DBEntry(DBEntry&&) = delete;

    ~DBEntry() {
        dbFinishEntry(&ent);
    }

    operator DBENTRY*() {
        return &ent;
    }

    DBENTRY* operator->() {
        return &ent;
    }

    const char* info(const char *key, const char* defval=nullptr) {
        const char *ret = defval;
        if(!dbFindInfo(&ent, key)) {
            ret = ent.pinfonode->string;
        }
        return ret;
    }
};

// Iterate every record currently loaded in the database, invoking fn(prec) for each.
template<typename Fn>
void forEachRecord(Fn&& fn) {
    DBEntry ent;
    for (long status = dbFirstRecordType(ent); !status; status = dbNextRecordType(ent))
        for (status = dbFirstRecord(ent); !status; status = dbNextRecord(ent))
            fn(static_cast<dbCommon*>(ent->precnode->precord));
}

} // ioc
} // pvxs
#endif //PVXS_DBENTRY_H
