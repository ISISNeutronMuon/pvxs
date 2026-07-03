/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <string>

#include <testMain.h>
#include <alarm.h>
#include <dbAccess.h>
#include <dbStaticLib.h>
#include <dbUnitTest.h>
#include <epicsExit.h>
#include <recGbl.h>

#include <pvxs/log.h>
#include <pvxs/client.h>
#include <pvxs/server.h>
#include <pvxs/unittest.h>
#include <pvxs/iochooks.h>

#include "dblocker.h"
#include "testioc.h"

extern "C" {
extern int testioc_registerRecordDeviceDriver(struct dbBase*);
}

using namespace pvxs;

namespace {

void testAlarmMessage()
{
    testDiag("%s", __func__);
    TestClient ctxt;

    // specific Q:HIHI_AMSG message
    testdbPutFieldOk("test:amsg", DBR_DOUBLE, 95.0);
    auto val = ctxt.get("test:amsg").exec()->wait(5.0);
    testStrEq(val["alarm.message"].as<std::string>(), "Too high!");

    // specific Q:HIGH_AMSG message
    testdbPutFieldOk("test:amsg", DBR_DOUBLE, 85.0);
    val = ctxt.get("test:amsg").exec()->wait(5.0);
    testStrEq(val["alarm.message"].as<std::string>(), "Getting high");

    // no alarm - message is empty
    testdbPutFieldOk("test:amsg", DBR_DOUBLE, 50.0);
    val = ctxt.get("test:amsg").exec()->wait(5.0);
    testStrEq(val["alarm.message"].as<std::string>(), "");

    // Q:LOLO_AMSG with a message longer than the 40-char DB_AMSG_SIZE limit
    testdbPutFieldOk("test:amsg", DBR_DOUBLE, 5.0);
    val = ctxt.get("test:amsg").exec()->wait(5.0);
    testStrEq(val["alarm.message"].as<std::string>(),
              "Value is critically low - this message exceeds forty chars");

    // LOW_ALARM with no specific Q:LOW_AMSG - falls back to Q:DEFAULT_AMSG
    testdbPutFieldOk("test:amsg", DBR_DOUBLE, 15.0);
    val = ctxt.get("test:amsg").exec()->wait(5.0);
    testStrEq(val["alarm.message"].as<std::string>(), "Something is wrong");

    // Q:DEFAULT_AMSG fallback when no specific key is set
    testdbPutFieldOk("test:amsg:default", DBR_DOUBLE, 95.0);
    val = ctxt.get("test:amsg:default").exec()->wait(5.0);
    testStrEq(val["alarm.message"].as<std::string>(), "Default message");
}

void testDefaultMsgUpdate()
{
    testDiag("%s", __func__);
    TestClient ctxt;

    // Confirm the initial default fallback message
    testdbPutFieldOk("test:amsg:default", DBR_DOUBLE, 95.0);
    auto val = ctxt.get("test:amsg:default").exec()->wait(5.0);
    testStrEq(val["alarm.message"].as<std::string>(), "Default message");

    // Update Q:DEFAULT_AMSG in the static database
    {
        DBENTRY ent;
        dbInitEntry(pdbbase, &ent);
        testOk(!dbFindRecord(&ent, "test:amsg:default"), "find record test:amsg:default");
        testOk(!dbFindInfo(&ent, "Q:DEFAULT_AMSG"), "find info Q:DEFAULT_AMSG");
        testOk(!dbPutInfoString(&ent, "Updated message"), "dbPutInfoString");
        dbFinishEntry(&ent);
    }

    // Re-fetch - postProcessNode reads defaultNode->string at call time, not at cache-build time
    val = ctxt.get("test:amsg:default").exec()->wait(5.0);
    testStrEq(val["alarm.message"].as<std::string>(), "Updated message");
}

void testDefaultMsgNotForUnmatchedAlarms()
{
    testDiag("%s", __func__);
    TestClient ctxt;

    // Q:DEFAULT_AMSG is only consulted as a fallback for the five statuses
    // applyAlarmMessage() switches on (HIHI/HIGH/LOLO/LOW/STATE) when their
    // own specific key is missing -- not for any other alarm status. For
    // those, alarm.message keeps the raw status name that IOCSource::get()
    // already wrote before postProcessNode() ran (see iocsource.cpp), e.g.
    // "READ" or "SCAN" -- NOT the empty string, and NOT Q:DEFAULT_AMSG.
    // READ_ALARM/SCAN_ALARM cannot be produced by a simple field PUT, so
    // drive STAT via recGblSetSevr()/recGblResetAlarms() instead, as device
    // support would.
    dbCommon* prec = testdbRecordPtr("test:amsg:unmatched");

    for (auto stat : {std::make_pair(READ_ALARM, "READ"), std::make_pair(SCAN_ALARM, "SCAN")}) {
        {
            ioc::DBLocker L(prec);
            recGblSetSevr(prec, stat.first, INVALID_ALARM);
            recGblResetAlarms(prec);
        }
        auto val = ctxt.get("test:amsg:unmatched").exec()->wait(5.0);
        testStrEq(val["alarm.message"].as<std::string>(), stat.second);
    }
}

void testAlarmMessageState()
{
    testDiag("%s", __func__);
    TestClient ctxt;

    for (int i = 0; i < 16; i++) {
        testdbPutFieldOk("test:amsg:state", DBR_LONG, i);
        auto val = ctxt.get("test:amsg:state").exec()->wait(5.0);
        testStrEq(val["alarm.message"].as<std::string>(),
                  "State " + std::to_string(i) + " alarm");
    }
}

} // namespace

MAIN(testalarmmsg)
{
    testPlan(53);
    testSetup();
    pvxs::logger_config_env();
    {
        ioc::TestIOC ioc;
        testdbReadDatabase("testioc.dbd", nullptr, nullptr);
        testOk1(!testioc_registerRecordDeviceDriver(pdbbase));
        testdbReadDatabase("testalarmmsg.db", nullptr, nullptr);
        ioc.init();
        testAlarmMessage();
        testDefaultMsgUpdate();
        testDefaultMsgNotForUnmatchedAlarms();
        testAlarmMessageState();
    }
    epicsExitCallAtExits();
    cleanup_for_valgrind();
    return testDone();
}
