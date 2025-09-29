TOP = ../..

include $(TOP)/configure/CONFIG

TESTPROD += test_diode
test_diode_SRCS += test_diode.cpp
test_diode_LIBS = Com epics-diode
TESTS += test_diode

TESTSCRIPTS_HOST += $(TESTS:%=%.t)
ifdef BASE_3_15
ifneq ($(filter $(T_A),$(CROSS_COMPILER_RUNTEST_ARCHS)),)
TESTPROD = $(TESTPROD_HOST)
TESTSCRIPTS = $(TESTSCRIPTS_HOST)
endif
endif

include $(TOP)/configure/RULES

