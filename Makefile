# Makefile for the epics-diode module

TOP = .
include $(TOP)/configure/CONFIG

DIRS += configure

DIRS += src
src_DEPEND_DIRS = configure

DIRS += ioc
ioc_DEPEND_DIRS = src

DIRS += test
test_DEPEND_DIRS = src ioc

include $(TOP)/configure/RULES_TOP
