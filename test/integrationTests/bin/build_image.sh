#!/bin/sh

cd `dirname $0`

cp -R ../../../configure ../images/epics-diode/epics-diode/
cp -R ../../../ioc ../images/epics-diode/epics-diode/
cp -R ../../../src ../images/epics-diode/epics-diode/
cp ../../../Makefile ../images/epics-diode/epics-diode/
mkdir ../images/epics-diode/epics-diode/test
cp -R ../../../test/testDiodeApp ../images/epics-diode/epics-diode/test/

docker build --rm \
    ../images/epics-diode -t epics-diode-img

