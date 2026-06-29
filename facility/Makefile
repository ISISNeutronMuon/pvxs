TOP=..
include $(TOP)/configure/CONFIG

include $(TOP)/configure/RULES_DIRS

# All registrar sources: every .cpp in facility/ except the generated file itself.
FACILITY_REGISTRAR_SRCS := $(filter-out \
    $(TOP)/facility/facilityregister.cpp, \
    $(sort $(wildcard $(TOP)/facility/*.cpp)))

# ioc/ picks up facilityregister.cpp via its SRC_DIRS wildcard; it must
# exist before the ioc build starts (enforced by ioc_DEPEND_DIRS = facility
# in the top-level Makefile).
all: $(TOP)/facility/facilityregister.cpp

$(TOP)/facility/facilityregister.cpp: $(FACILITY_REGISTRAR_SRCS) $(TOP)/facility/gen_facilityregister.py
	python3 $(TOP)/facility/gen_facilityregister.py \
	    $(FACILITY_REGISTRAR_SRCS) > $@.tmp && mv $@.tmp $@

.PHONY: clean_generated
clean: clean_generated
clean_generated:
	$(RM) $(TOP)/facility/facilityregister.cpp
