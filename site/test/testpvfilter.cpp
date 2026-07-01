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

#include "sitehooks.h"

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

    // No Q:pv:disable info field - record is served normally
    auto val = ctxt.get("test:normal").exec()->wait(5.0);
    testTrue(!!val) << "test:normal is served";

    // Q:pv:disable=1 - record must not be served
    testThrows<client::Timeout>([&ctxt]() {
        ctxt.get("test:disabled").exec()->wait(2.0);
    }) << "test:disabled is not served";

    // Q:pv:disable=0 - treated as enabled, record is served
    val = ctxt.get("test:enabled0").exec()->wait(5.0);
    testTrue(!!val) << "test:enabled0 is served";
}

void testQLoopbackOnly()
{
    testDiag("%s", __func__);
    TestClient ctxt;

    // Q:pv:loopback_only=1 -- test client always connects via loopback, so access is allowed
    auto val = ctxt.get("test:loopback_only").exec()->wait(5.0);
    testTrue(!!val) << "test:loopback_only accessible from loopback client";

    // Q:pv:loopback_only=0 -- opt-out: treated as unrestricted
    val = ctxt.get("test:loopback_enabled0").exec()->wait(5.0);
    testTrue(!!val) << "test:loopback_enabled0 is served";

    // Verify the deny path directly: non-loopback addresses must be rejected
    testFalse(ioc::site::isChannelAllowed("test:loopback_only", "192.168.1.100:5076"))
        << "loopback_only denied for non-loopback IPv4";
    testTrue(ioc::site::isChannelAllowed("test:loopback_only", "127.0.0.1:5076"))
        << "loopback_only allowed for 127.0.0.1";
    testFalse(ioc::site::isChannelAllowed("test:loopback_only", "[2001:db8::1]:5076"))
        << "loopback_only denied for non-loopback IPv6";
    testTrue(ioc::site::isChannelAllowed("test:loopback_only", "[::1]:5076"))
        << "loopback_only allowed for ::1";

    // nullptr peer means name-list construction: loopback_only records must remain visible
    testTrue(ioc::site::isChannelAllowed("test:loopback_only", nullptr))
        << "loopback_only kept in name list (nullptr peer)";
}

} // namespace

MAIN(testpvfilter)
{
    testPlan(11);
    testSetup();
    pvxs::logger_config_env();
    {
        ioc::TestIOC ioc;
        testdbReadDatabase("testioc.dbd", nullptr, nullptr);
        testOk1(!testioc_registerRecordDeviceDriver(pdbbase));
        testdbReadDatabase("testpvfilter.db", nullptr, nullptr);
        ioc.init();
        testQDisable();
        testQLoopbackOnly();
    }
    epicsExitCallAtExits();
    cleanup_for_valgrind();
    return testDone();
}
