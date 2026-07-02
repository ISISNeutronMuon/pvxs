/*
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvxs is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <cmath>

#include <testMain.h>
#include <dbAccess.h>
#include <dbUnitTest.h>
#include <epicsExit.h>

#include <pvxs/log.h>
#include <pvxs/client.h>
#include <pvxs/server.h>
#include <pvxs/unittest.h>
#include <pvxs/iochooks.h>

#include "testioc.h"

extern "C" {
extern int testioc_registerRecordDeviceDriver(struct dbBase*);
}

using namespace pvxs;

namespace {

void testDefaultPrec()
{
    testDiag("%s", __func__);
    TestClient ctxt;

    // floating-point record with PREC=0 (default) - precision raised to 2
    auto val = ctxt.get("test:dp:float").exec()->wait(5.0);
    testOk(val["display.precision"].as<int32_t>() == 2,
           "PREC=0 float: precision defaulted to 2");

    // Q:PREC_ZERO=1 opts out - precision stays 0
    val = ctxt.get("test:dp:preczero").exec()->wait(5.0);
    testOk(val["display.precision"].as<int32_t>() == 0,
           "PREC=0, Q:PREC_ZERO=1: precision stays 0");

    // explicit PREC=3 - not zero, so never modified
    val = ctxt.get("test:dp:prec3").exec()->wait(5.0);
    testOk(val["display.precision"].as<int32_t>() == 3,
           "PREC=3: precision unchanged at 3");

    // integer record (Kind::Integer) - not modified regardless of precision value
    val = ctxt.get("test:dp:int").exec()->wait(5.0);
    testOk(val["display.precision"].as<int32_t>() == 0,
           "integer record: precision unchanged at 0");

    // Q:PREC_ZERO=0 is treated as disabled - opt-out inactive, precision raised to 2
    val = ctxt.get("test:dp:zero0").exec()->wait(5.0);
    testOk(val["display.precision"].as<int32_t>() == 2,
           "PREC=0, Q:PREC_ZERO=0: opt-out inactive, precision defaulted to 2");

    // double waveform (Kind::Real array) - also raised to 2
    val = ctxt.get("test:dp:wf").exec()->wait(5.0);
    testOk(val["display.precision"].as<int32_t>() == 2,
           "double waveform PREC=0: precision defaulted to 2");

    // calc record (double value, Kind::Real) - treated identically to ai
    val = ctxt.get("test:dp:calc").exec()->wait(5.0);
    testOk(val["display.precision"].as<int32_t>() == 2,
           "calc PREC=0: precision defaulted to 2");
    // display.precision change must not affect the computed value (A+B = 1.23456 + 7.89012)
    testOk(std::fabs(val["value"].as<double>() - 9.12468) < 1e-9,
           "calc value: A+B unchanged at 9.12468");
}

} // namespace

MAIN(testdefaultprec)
{
    testPlan(9);
    testSetup();
    pvxs::logger_config_env();
    {
        ioc::TestIOC ioc;
        testdbReadDatabase("testioc.dbd", nullptr, nullptr);
        testOk1(!testioc_registerRecordDeviceDriver(pdbbase));
        testdbReadDatabase("testdefaultprec.db", nullptr, nullptr);
        ioc.init();
        testDefaultPrec();
    }
    epicsExitCallAtExits();
    cleanup_for_valgrind();
    return testDone();
}
