TOP=../..

include $(TOP)/configure/CONFIG

USR_CPPFLAGS += -I$(TOP)/ioc
SRC_DIRS += $(TOP)/site_specific

ifdef BASE_3_15

DBDDEPENDS_FILES += testioc.dbd$(DEP)
testioc_DBD = base.dbd pvxsIoc.dbd
TESTFILES += $(COMMON_DIR)/testioc.dbd

ifneq ($(wildcard $(TOP)/site_specific/qdisable.cpp),)
TESTPROD_HOST += testqdisable
testqdisable_SRCS += testqdisable.cpp
testqdisable_SRCS += qdisable.cpp
testqdisable_SRCS += testioc_registerRecordDeviceDriver.cpp
testqdisable_LIBS += pvxsIoc pvxs $(EPICS_BASE_IOC_LIBS)
TESTS += testqdisable
TESTFILES += ../testqdisable.db
endif

ifneq ($(wildcard $(TOP)/site_specific/alarmmsg.cpp),)
TESTPROD_HOST += testalarmmsg
testalarmmsg_SRCS += testalarmmsg.cpp
testalarmmsg_SRCS += alarmmsg.cpp
testalarmmsg_SRCS += testioc_registerRecordDeviceDriver.cpp
testalarmmsg_LIBS += pvxsIoc pvxs $(EPICS_BASE_IOC_LIBS)
TESTS += testalarmmsg
TESTFILES += ../testalarmmsg.db
endif

endif # BASE_3_15

TESTSCRIPTS_HOST += $(TESTS:%=%.t)
ifdef BASE_3_15
ifneq ($(filter $(T_A),$(CROSS_COMPILER_RUNTEST_ARCHS)),)
TESTPROD = $(TESTPROD_HOST)
TESTSCRIPTS = $(TESTSCRIPTS_HOST)
endif
endif

include $(TOP)/configure/RULES
