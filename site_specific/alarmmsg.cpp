/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <string.h>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <utility>

#include <pvxs/log.h>

#include <epicsTypes.h>
#include <alarm.h>
#include <dbStaticLib.h>

#include "dbentry.h"
#include "sitehooks.h"

// include last to avoid clash of #define printf with other headers
#include <epicsStdio.h>

DEFINE_LOGGER(_log, "pvxs.ioc.db");

namespace {

struct InfoCache {
    std::vector<std::pair<const char*, dbInfoNode*>> fields;
    const char* defaultMsg = nullptr;
};

std::unordered_map<dbCommon*, InfoCache> s_infoCache;

InfoCache& buildAndCache(dbCommon* prec)
{
    InfoCache& cached = s_infoCache[prec];
    pvxs::ioc::DBEntry ent(prec);

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

const char* findQInfoValue(const InfoCache& cache, const char* key)
{
    auto cmp = [](const std::pair<const char*, dbInfoNode*>& entry, const char* k) {
        return strcmp(entry.first, k) < 0;
    };
    auto it = std::lower_bound(cache.fields.begin(), cache.fields.end(), key, cmp);
    if (it != cache.fields.end() && strcmp(it->first, key) == 0)
        return it->second->string;
    return cache.defaultMsg;
}

void applyAlarmMessage(dbCommon* prec, pvxs::Value& node)
{
    if (prec->stat == NO_ALARM)
        return;

    auto it = s_infoCache.find(prec);
    if (it == s_infoCache.end() || it->second.fields.empty())
        return;

    const InfoCache& cache = it->second;
    const char* stsmsg = nullptr;

    switch(prec->stat) {
    case HIHI_ALARM:
        stsmsg = findQInfoValue(cache, "Q:HIHI_AMSG");
        break;
    case HIGH_ALARM:
        stsmsg = findQInfoValue(cache, "Q:HIGH_AMSG");
        break;
    case LOLO_ALARM:
        stsmsg = findQInfoValue(cache, "Q:LOLO_AMSG");
        break;
    case LOW_ALARM:
        stsmsg = findQInfoValue(cache, "Q:LOW_AMSG");
        break;
    case STATE_ALARM: {
        auto index = node["value.index"].as<int32_t>();
        if (index >= 0) {
            char buf[32];
            epicsSnprintf(buf, sizeof(buf), "Q:STATE%d_AMSG", index);
            stsmsg = findQInfoValue(cache, buf);
        }
        break;
    }
    default:
        break;
    }

    if (stsmsg)
        node["alarm.message"] = stsmsg;
}

void onBeginning()
{
    s_infoCache.clear();
}

void onIocBuilt()
{
    pvxs::ioc::DBEntry ent;
    for (long status = dbFirstRecordType(ent); !status; status = dbNextRecordType(ent)) {
        for (status = dbFirstRecord(ent); !status; status = dbNextRecord(ent)) {
            buildAndCache(static_cast<dbCommon*>(ent->precnode->precord));
        }
    }
    log_debug_printf(_log, "alarm msg cache populated: %zu records\n", s_infoCache.size());
}

struct Registrar {
    Registrar() {
        pvxs::ioc::site::addInitHookAtBeginning(onBeginning);
        pvxs::ioc::site::addInitHookAfterIocBuilt(onIocBuilt);
        pvxs::ioc::site::setNodePostProcessor(applyAlarmMessage);
    }
} s_registrar;

} // namespace
