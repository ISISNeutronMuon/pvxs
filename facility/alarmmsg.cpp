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

// Per-record cache of Q:*_AMSG info nodes, built once at initHookAfterIocBuilt.
struct InfoCache {
    // Sorted by info-node name for O(log n) lookup.
    std::vector<std::pair<const char*, dbInfoNode*>> fields;
    // Stored as dbInfoNode* rather than const char* so that ->string is read at
    // call time; dbPutInfoString() rewrites the string in place within the same
    // node, so the pointer remains valid and reflects runtime changes without
    // any cache rebuild.
    dbInfoNode* defaultNode = nullptr;

    const char* defaultMessage() const {
        return defaultNode ? defaultNode->string : nullptr;
    }

    /** Return the string for @p key, or the Q:DEFAULT_AMSG fallback if absent. */
    const char* lookup(const char* key) const {
        auto cmp = [](const std::pair<const char*, dbInfoNode*>& entry, const char* k) {
            return strcmp(entry.first, k) < 0;
        };
        auto it = std::lower_bound(fields.begin(), fields.end(), key, cmp);
        if (it != fields.end() && strcmp(it->first, key) == 0)
            return it->second->string;
        return defaultMessage();
    }
};

std::unordered_map<dbCommon*, InfoCache> s_infoCache;

/**
 * Scan all Q:* info fields on @p prec, build a sorted InfoCache, and store it.
 * Called for every record at initHookAfterIocBuilt.
 */
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
        cached.defaultNode = def->second;

    return cached;
}

/**
 * Node post-processor: write alarm.message from Q:*_AMSG info fields.
 * Called by IOCSource::get() with the record locked; prec->stat is stable.
 *
 * Alarm-status-to-info-key mapping:
 *   HIHI → Q:HIHI_AMSG, HIGH → Q:HIGH_AMSG,
 *   LOLO → Q:LOLO_AMSG, LOW  → Q:LOW_AMSG,
 *   STATE → Q:STATE<n>_AMSG  (n = value.index from the PVA node, not the record)
 * Any unmatched alarm or missing key falls back to Q:DEFAULT_AMSG.
 *
 * @param prec record pointer (locked)
 * @param node PVA value node to be returned to the client
 */
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
        stsmsg = cache.lookup("Q:HIHI_AMSG");
        break;
    case HIGH_ALARM:
        stsmsg = cache.lookup("Q:HIGH_AMSG");
        break;
    case LOLO_ALARM:
        stsmsg = cache.lookup("Q:LOLO_AMSG");
        break;
    case LOW_ALARM:
        stsmsg = cache.lookup("Q:LOW_AMSG");
        break;
    case STATE_ALARM: {
        // Use value.index from the PVA node rather than any record field;
        // the enum value is already encoded in node by IOCSource::get().
        auto index = node["value.index"].as<int32_t>();
        if (index >= 0) {
            char buf[32];
            epicsSnprintf(buf, sizeof(buf), "Q:STATE%d_AMSG", index);
            stsmsg = cache.lookup(buf);
        }
        break;
    }
    default:
        break;
    }

    if (stsmsg)
        node["alarm.message"] = stsmsg;
}

// Clear the cache so it is rebuilt cleanly on the next iocInit().
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
