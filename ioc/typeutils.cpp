/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *
 * Author George S. McIntyre <george@level-n.com>, 2023
 *
 */

#include <string.h>
#include <algorithm>
#include <unordered_map>

#include <pvxs/source.h>
#include <pvxs/log.h>

#include <dbStaticLib.h>
#include <epicsStdlib.h>

#include "dbentry.h"
#include "fielddefinition.h"
#include "typeutils.h"

DEFINE_LOGGER(_log, "pvxs.ioc.db");

namespace {

struct InfoCache {
    std::vector<std::pair<const char*, dbInfoNode*>> fields;
    const char* defaultMsg = nullptr;
};

std::unordered_map<dbCommon*, InfoCache> s_infoCache;

} // namespace

namespace pvxs {

/**
 * Convert the given database record type code into a pvxs type code
 *
 * @param dbrType the database record type code
 * @return a pvxs type code
 *
 */
TypeCode fromDbrType(short dbrType) {
    switch (dbrType) {
    case DBR_CHAR:
        return TypeCode::Int8;
    case DBR_UCHAR:
        return TypeCode::UInt8;
    case DBR_SHORT:
        return TypeCode::Int16;
    case DBR_USHORT:
    case DBR_ENUM:
        return TypeCode::UInt16;
    case DBR_LONG:
        return TypeCode::Int32;
    case DBR_ULONG:
        return TypeCode::UInt32;
#ifdef DBR_INT64
    case DBR_INT64:
        return TypeCode::Int64;
    case DBR_UINT64:
        return TypeCode::UInt64;
#endif
    case DBR_FLOAT:
        return TypeCode::Float32;
    case DBR_DOUBLE:
        return TypeCode::Float64;
    case DBR_STRING:
        return TypeCode::String;
    case DBR_NOACCESS:
    default:
        return TypeCode::Null;
    }
}


namespace ioc {
const char *MappingInfo::name(type_t t)
{
    switch(t) {
    case Scalar: return "scalar";
    case Plain: return "plain";
    case Any: return "any";
    case Meta: return "meta";
    case Proc: return "proc";
    case Structure: return "structure";
    case Const: return "const";
    }
    return "<invalid>";
}

void MappingInfo::updateNsecMask(dbCommon *prec)
{
    assert(prec);
    DBEntry ent(prec);
    if(auto val = ent.info("Q:time:tag")) {
        epicsInt32 dig = 0;
        if(strncmp(val, "nsec:lsb:", 9)==0 && !epicsParseInt32(&val[9], &dig, 10, nullptr)) {
            nsecMask = (uint64_t(1u)<<dig)-1u;
        }
    }
}

/**
 * Populate the infoFields cache from the info nodes of the given record.
 *
 * Only info fields whose names begin with the "Q:" prefix are stored.
 * All other info fields are ignored. Subsequent code (e.g. alarm message
 * lookup) must therefore only query infoFields for Q:-prefixed keys.
 *
 * @param prec the record whose info nodes are to be cached
 */
static InfoCache& buildAndCache(dbCommon* prec)
{
    InfoCache& cached = s_infoCache[prec];
    DBEntry ent(prec);

    for (auto status = dbFirstInfo(ent); !status; status = dbNextInfo(ent)) {
        if (strncmp(ent->pinfonode->name, "Q:", 2) == 0)
            cached.fields.emplace_back(ent->pinfonode->name, ent->pinfonode);
    }
    std::sort(cached.fields.begin(), cached.fields.end(),
              [](const std::pair<const char*, dbInfoNode*>& a,
                 const std::pair<const char*, dbInfoNode*>& b) {
                  return strcmp(a.first, b.first) < 0;
              });

    auto cmp = [](const std::pair<const char*, dbInfoNode*>& entry, const char* key) {
        return strcmp(entry.first, key) < 0;
    };
    auto def = std::lower_bound(cached.fields.begin(), cached.fields.end(), "Q:DEFAULT_AMSG", cmp);
    if (def != cached.fields.end() && strcmp(def->first, "Q:DEFAULT_AMSG") == 0)
        cached.defaultMsg = def->second->string;

    return cached;
}

void MappingInfo::updateInfoFields(dbCommon *prec)
{
    assert(prec);

    auto it = s_infoCache.find(prec);
    const InfoCache& cached = (it != s_infoCache.end()) ? it->second : buildAndCache(prec);

    infoFields = cached.fields;
    defaultAlarmMsg = cached.defaultMsg;
    log_debug_printf(_log, "updateInfoFields: %s (%zu fields)\n", prec->name, infoFields.size());
}

void MappingInfo::populateInfoFieldsCache()
{
    s_infoCache.clear();  // discard any stale entries from a previous IOC run

    DBEntry ent;
    for (long status = dbFirstRecordType(ent); !status; status = dbNextRecordType(ent)) {
        for (status = dbFirstRecord(ent); !status; status = dbNextRecord(ent)) {
            buildAndCache(static_cast<dbCommon*>(ent->precnode->precord));
        }
    }

    log_debug_printf(_log, "populateInfoFieldsCache: cached %zu records\n", s_infoCache.size());
}

const char* MappingInfo::findAlarmMsg(const char* key) const
{
    auto cmp = [](const std::pair<const char*, dbInfoNode*>& entry, const char* k) {
        return strcmp(entry.first, k) < 0;
    };
    auto it = std::lower_bound(infoFields.begin(), infoFields.end(), key, cmp);
    if (it != infoFields.end() && strcmp(it->first, key) == 0)
        return it->second->string;
    return defaultAlarmMsg;
}

} // namespace ioc

} // namespace pvxs
