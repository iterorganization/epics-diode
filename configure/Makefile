# Makefile in extensions/configure directory

TOP = ..

include $(TOP)/configure/CONFIG

CFG += CONFIG_EPICS_DIODE_VERSION

TARGETS = $(CONFIG_TARGETS)
CONFIGS += $(subst ../,,$(wildcard $(CONFIG_INSTALLS)))

include $(TOP)/configure/RULES

