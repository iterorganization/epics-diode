#!/bin/sh

cd `dirname $0`

docker exec \
    modules-xpoz-1 /bin/bash -o pipefail -c "
        export EPICS_CA_AUTO_ADDR_LIST="no"
        export EPICS_CA_ADDR_LIST="xpoz"
        cd /test_bin/destIoc &&
        screen -S ioc -d -m /epics-diode/bin/linux-x86_64/testDiode xpoz.cmd &&
        sleep 15 ;
        exit "'$?'"
    "

# return last exit code = docker exit code = last command inside docker exit code
exit "$?"
