#!/bin/sh

cd `dirname $0`

docker exec \
    modules-poz-1 /bin/bash -o pipefail -c "
        export EPICS_CA_AUTO_ADDR_LIST=no
        export EPICS_CA_ADDR_LIST=poz
        cd /epics-diode/bin/linux-x86_64 &&
        screen -S diode_sender -d -m ./diode_sender -c /test_config/diode.json xpoz:5080 &&
        sleep 10 &&
        exit "'$?'"
    "

# return last exit code = docker exit code = last command inside docker exit code
exit "$?"
