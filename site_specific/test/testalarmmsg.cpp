/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <string>

#include <testMain.h>
#include <dbAccess.h>
#include <dbUnitTest.h>
#include <epicsExit.h>

#include <pvxs/log.h>
#include <pvxs/client.h>
#include <pvxs/server.h>
#include <pvxs/unittest.h>
#include <pvxs/iochooks.h>

extern "C" {
extern int testioc_registerRecordDeviceDriver(struct dbBase*);
}

using namespace pvxs;

namespace {

struct TestClient : client::Context {
    TestClient() : client::Context(ioc::server().clientConfig().build()) {}
};

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

    // no alarm — message is empty
    testdbPutFieldOk("test:amsg", DBR_DOUBLE, 50.0);
    val = ctxt.get("test:amsg").exec()->wait(5.0);
    testStrEq(val["alarm.message"].as<std::string>(), "");

    // Q:LOLO_AMSG with a message longer than the 40-char DB_AMSG_SIZE limit
    testdbPutFieldOk("test:amsg", DBR_DOUBLE, 5.0);
    val = ctxt.get("test:amsg").exec()->wait(5.0);
    testStrEq(val["alarm.message"].as<std::string>(),
              "Value is critically low - this message exceeds forty chars");

    // LOW_ALARM with no specific Q:LOW_AMSG — falls back to Q:DEFAULT_AMSG
    testdbPutFieldOk("test:amsg", DBR_DOUBLE, 15.0);
    val = ctxt.get("test:amsg").exec()->wait(5.0);
    testStrEq(val["alarm.message"].as<std::string>(), "Something is wrong");

    // Q:DEFAULT_AMSG fallback when no specific key is set
    testdbPutFieldOk("test:amsg:default", DBR_DOUBLE, 95.0);
    val = ctxt.get("test:amsg:default").exec()->wait(5.0);
    testStrEq(val["alarm.message"].as<std::string>(), "Default message");
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
    testPlan(45);
    testSetup();
    pvxs::logger_config_env();
    {
        ioc::TestIOC ioc;
        testdbReadDatabase("testioc.dbd", nullptr, nullptr);
        testOk1(!testioc_registerRecordDeviceDriver(pdbbase));
        testdbReadDatabase("testalarmmsg.db", nullptr, nullptr);
        ioc.init();
        testAlarmMessage();
        testAlarmMessageState();
    }
    epicsExitCallAtExits();
    cleanup_for_valgrind();
    return testDone();
}
