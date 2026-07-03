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

#include "capturestd.h"
#include "sitehooks.h"
#include "testioc.h"

extern "C" {
extern int testioc_registerRecordDeviceDriver(struct dbBase*);
}

using namespace pvxs;

namespace {

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

// Verify that adding a Q:pv:disable / Q:pv:loopback_only record to a group
// still prints a warning at group-processing time (during ioc.init()), for
// visibility even though (unlike the record's own individual PV) the group
// now enforces these filters itself, per-field, at get/put/subscribe time.
void testGroupWarning(const std::string& err)
{
    testDiag("%s", __func__);
    testTrue(err.find("test:grp.disabled") != std::string::npos
             && err.find("Q:pv:disable") != std::string::npos
             && err.find("test:disabled") != std::string::npos)
        << "warns when a Q:pv:disable record is added to a group";
    testTrue(err.find("test:grp.loopback") != std::string::npos
             && err.find("Q:pv:loopback_only") != std::string::npos
             && err.find("test:loopback_only") != std::string::npos)
        << "warns when a Q:pv:loopback_only record is added to a group";
}

// Verify that Q:pv:disable is enforced per-field within a group: the group
// itself remains fully accessible, but the disabled field is blanked on GET,
// rejects PUT, and is excluded from monitor updates -- rather than the whole
// group being denied or the filter being bypassed entirely.
void testGroupFiltering()
{
    testDiag("%s", __func__);
    TestClient ctxt;

    auto val = ctxt.get("test:grp").exec()->wait(5.0);
    testTrue(!!val) << "test:grp (containing a disabled field) is still served";
    testEq(val["disabled.value"].as<double>(), 0.0)
        << "disabled field is blanked in the group, not the record's real value (12345)";

    testdbGetFieldEqual("test:disabled", DBR_DOUBLE, 12345.0);

    testThrows<client::RemoteError>([&ctxt]() {
        ctxt.put("test:grp").set("disabled.value", 99.0).exec()->wait(5.0);
    }) << "put to the disabled field through the group is rejected";
    testdbGetFieldEqual("test:disabled", DBR_DOUBLE, 12345.0);

    TestSubscription sub(ctxt.monitor("test:grp"));
    auto mval = sub.waitForUpdate();
    testTrue(!!mval) << "group monitor still primes with a disabled field present";
}

} // namespace

MAIN(testpvfilter)
{
    testPlan(19);
    testSetup();
    pvxs::logger_config_env();
    {
        ioc::TestIOC ioc;
        testdbReadDatabase("testioc.dbd", nullptr, nullptr);
        testOk1(!testioc_registerRecordDeviceDriver(pdbbase));
        testdbReadDatabase("testpvfilter.db", nullptr, nullptr);

        CaptureStd cap([&ioc](){
            ioc.init();
        });
        testGroupWarning(cap.err());

        testQDisable();
        testQLoopbackOnly();
        testGroupFiltering();
    }
    epicsExitCallAtExits();
    cleanup_for_valgrind();
    return testDone();
}
