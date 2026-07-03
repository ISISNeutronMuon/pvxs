/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <alarm.h>
#include <epicsTypes.h>

#include <pvxs/log.h>

#include "dbinfocache.h"
#include "sitehooks.h"

// include last to avoid clash of #define printf with other headers
#include <epicsStdio.h>

DEFINE_LOGGER(_log, "pvxs.ioc.db");

namespace {

// Function-local static avoids a file-scope static constructor, which would
// fail the CDT check (.ci-local/cdt-check.sh) on Linux.
pvxs::ioc::site::DbInfoCache& infoCache() {
    static pvxs::ioc::site::DbInfoCache s;
    return s;
}

/**
 * Node post-processor: write alarm.message from Q:*_AMSG info fields.
 * Called by IOCSource::get() with the record locked; prec->stat is stable.
 *
 * Alarm-status-to-info-key mapping:
 *   HIHI -> Q:HIHI_AMSG, HIGH -> Q:HIGH_AMSG,
 *   LOLO -> Q:LOLO_AMSG, LOW  -> Q:LOW_AMSG,
 *   STATE -> Q:STATE<n>_AMSG  (n = value.index from the PVA node, not the record)
 * Q:DEFAULT_AMSG is only consulted as a fallback for these five statuses,
 * when their own specific key is missing -- e.g. HIGH_ALARM with no
 * Q:HIGH_AMSG set. Any other alarm status (READ_ALARM, SCAN_ALARM, etc.,
 * not listed above) is left alone: alarm.message is not touched here at
 * all, regardless of whether Q:DEFAULT_AMSG is set. It keeps whatever
 * IOCSource::get() already wrote before calling this post-processor --
 * the raw alarm status name (e.g. "READ", "SCAN"), not the empty string.
 */
void applyAlarmMessage(dbCommon* prec, pvxs::Value& node)
{
    if (prec->stat == NO_ALARM)
        return;

    const auto* cache = infoCache().find(prec);
    if (!cache)
        return;

    const char* stsmsg = nullptr;

    switch(prec->stat) {
    case HIHI_ALARM: stsmsg = cache->lookup("Q:HIHI_AMSG"); break;
    case HIGH_ALARM: stsmsg = cache->lookup("Q:HIGH_AMSG"); break;
    case LOLO_ALARM: stsmsg = cache->lookup("Q:LOLO_AMSG"); break;
    case LOW_ALARM:  stsmsg = cache->lookup("Q:LOW_AMSG");  break;
    case STATE_ALARM: {
        // Use value.index from the PVA node rather than any record field;
        // the enum value is already encoded in node by IOCSource::get().
        auto index = node["value.index"].as<int32_t>();
        if (index >= 0) {
            char buf[32];
            epicsSnprintf(buf, sizeof(buf), "Q:STATE%d_AMSG", index);
            stsmsg = cache->lookup(buf);
        }
        break;
    }
    default: break;
    }

    if (stsmsg)
        node["alarm.message"] = stsmsg;
}

void onBeginning() { infoCache().clear(); }

void onIocBuilt() {
    infoCache().buildAll("Q:", "Q:DEFAULT_AMSG");
    log_debug_printf(_log, "alarm msg cache populated: %zu records\n", infoCache().size());
}

} // namespace

namespace pvxs { namespace ioc { namespace site {
void registerAlarmmsg() {
    addInitHookAtBeginning(onBeginning);
    addInitHookAfterIocBuilt(onIocBuilt);
    addNodePostProcessor(applyAlarmMessage);
}
}}} // pvxs::ioc::site
