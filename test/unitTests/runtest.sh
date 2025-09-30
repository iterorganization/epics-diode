#!/bin/bash

cd `dirname $0`
MYDIR=`pwd`

export LD_LIBRARY_PATH="$MYDIR/../../lib/$EPICS_HOST_ARCH"

cd 'O.$EPICS_HOST_ARCH'
exec ./test_diode && ./test_sender_receiver

exit $?


