#!/bin/sh

export EPICS_BASE=/opt/codac-7.2/epics
export EPICS_HOST_ARCH=linux-x86_64
export EPICS_ROOT=/opt/codac-7.2/epics

cd /epics-diode
echo EPICS_BASE=$EPICS_BASE > configure/RELEASE.local

make distclean
make

cp lib/$EPICS_HOST_ARCH/* $EPICS_BASE/lib/$EPICS_HOST_ARCH/
ldconfig

sleep infinity

