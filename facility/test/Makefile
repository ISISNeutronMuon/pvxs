TOP=../..

include $(TOP)/configure/CONFIG

USR_CPPFLAGS += -I$(TOP)/ioc
SRC_DIRS += $(TOP)/facility

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES
