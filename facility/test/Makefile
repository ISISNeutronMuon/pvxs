TOP=../..

include $(TOP)/configure/CONFIG

USR_CPPFLAGS += -I$(TOP)/ioc
SRC_DIRS += $(TOP)/facility

DBDDEPENDS_FILES += testioc.dbd$(DEP)
testioc_DBD = base.dbd pvxsIoc.dbd
TESTFILES += $(COMMON_DIR)/testioc.dbd

define add_test
TESTPROD_HOST += test$(1)
test$(1)_SRCS += test$(1).cpp
test$(1)_SRCS += $(1).cpp
test$(1)_SRCS += testioc_registerRecordDeviceDriver.cpp
test$(1)_LIBS += pvxsIoc pvxs $$(EPICS_BASE_IOC_LIBS)
TESTS += test$(1)
endef

# Facility tests use TestIOC and dbUnitTest.h, both of which require base >= 3.15.
ifdef BASE_3_15

# For each <name>.cpp in facility/, add a test binary if test<name>.cpp exists here.
# Does not handle tests that need extra source files, extra libs, or a different
# binary name than test<name>; add those manually below.
#
# Note: the optional TESTFILES line for the .db fixture is NOT inside add_test.
# $$(if $$(wildcard ...)) inside a define passes $(if ...) as a literal string to
# eval, but eval only parses — it does not re-expand function calls in the text it
# receives.  Keeping the $(if $(wildcard ...)) here in the _unused := line means
# it is expanded by := before eval ever sees it, avoiding a "missing separator" error.
_unused := $(foreach src,$(notdir $(wildcard $(TOP)/facility/*.cpp)),\
  $(if $(wildcard ../test$(basename $(src)).cpp),\
    $(eval $(call add_test,$(basename $(src))))\
    $(if $(wildcard ../test$(basename $(src)).db),\
      $(eval TESTFILES += ../test$(basename $(src)).db))))

endif # BASE_3_15

TESTSCRIPTS_HOST += $(TESTS:%=%.t)
# No CROSS_COMPILER_RUNTEST_ARCHS block here: these tests use a PVAccess
# client/server pair and require a working loopback network, which is not
# available in the RTEMS QEMU (PC686) emulation environment.  The main
# test/Makefile has the same block but its self-contained (non-IOC) tests
# do work there; facility tests do not.

include $(TOP)/configure/RULES
