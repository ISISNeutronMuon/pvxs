/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <cstring>
#include <unordered_set>

#include "dbentry.h"
#include "sitehooks.h"

namespace {

std::unordered_set<dbCommon*>& precZeroSet() {
    static std::unordered_set<dbCommon*> s;
    return s;
}

void buildPrecZeroSet()
{
    auto& s = precZeroSet();
    s.clear();
    pvxs::ioc::DBEntry ent;
    for (long status = dbFirstRecordType(ent); !status; status = dbNextRecordType(ent)) {
        for (status = dbFirstRecord(ent); !status; status = dbNextRecord(ent)) {
            auto* prec = static_cast<dbCommon*>(ent->precnode->precord);
            pvxs::ioc::DBEntry infoEnt(prec);
            const char* val = infoEnt.info("Q:PREC_ZERO");
            if (val && val[0] != '\0' && strcmp(val, "0") != 0)
                s.insert(prec);
        }
    }
}

// Set display.precision to 2 when a floating-point record has PREC=0 (the default).
// Records with info(Q:PREC_ZERO, "1") are exempted, preserving precision=0 as set.
// Kind::Real covers Float32, Float64, and their array variants; integer and enum
// records are left untouched so their precision=0 remains meaningful.
void applyDefaultPrecision(dbCommon* prec, pvxs::Value& node)
{
    auto val = node["value"];
    if (!val || val.type().kind() != pvxs::Kind::Real)
        return;
    if (precZeroSet().count(prec))
        return;
    if (auto fld = node["display.precision"]) {
        if (fld.as<int32_t>() == 0)
            fld = int32_t(2);
    }
}

} // namespace

namespace pvxs { namespace ioc { namespace site {
void registerDefaultprec() {
    addInitHookAtBeginning(buildPrecZeroSet);
    addNodePostProcessor(applyDefaultPrecision);
}
}}} // pvxs::ioc::site
