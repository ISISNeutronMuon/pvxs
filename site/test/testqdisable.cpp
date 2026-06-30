/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

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

void testQDisable()
{
    testDiag("%s", __func__);
    TestClient ctxt;

    // No Q:DISABLE info field - record is served normally
    auto val = ctxt.get("test:normal").exec()->wait(5.0);
    testTrue(!!val) << "test:normal is served";

    // Q:DISABLE=1 - record must not be served
    testThrows<client::Timeout>([&ctxt]() {
        ctxt.get("test:disabled").exec()->wait(2.0);
    }) << "test:disabled is not served";

    // Q:DISABLE=0 - treated as enabled, record is served
    val = ctxt.get("test:enabled0").exec()->wait(5.0);
    testTrue(!!val) << "test:enabled0 is served";
}

} // namespace

MAIN(testqdisable)
{
    testPlan(4);
    testSetup();
    pvxs::logger_config_env();
    {
        ioc::TestIOC ioc;
        testdbReadDatabase("testioc.dbd", nullptr, nullptr);
        testOk1(!testioc_registerRecordDeviceDriver(pdbbase));
        testdbReadDatabase("testqdisable.db", nullptr, nullptr);
        ioc.init();
        testQDisable();
    }
    epicsExitCallAtExits();
    cleanup_for_valgrind();
    return testDone();
}
