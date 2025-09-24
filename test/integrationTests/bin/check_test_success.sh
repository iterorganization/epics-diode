#!/bin/sh

cd `dirname $0`

# Scan for failed tests
RESULT=`find "../volumes/test/" -name failed | wc -l`

exit $RESULT

