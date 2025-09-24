#!/bin/sh

cd `dirname $0`

docker exec \
    modules-xpoz-1 /bin/bash -o pipefail -c "
        export EPICS_CA_AUTO_ADDR_LIST="no"
        export EPICS_CA_ADDR_LIST="xpoz"
        screen -S monitor -d -m -L -Logfile /test_output/monitor.out \
            /test_bin/destIoc/screen_monitor_command.sh && \
        sleep 10 ;
        exit "'$?'"
    "

# return last exit code = docker exit code = last command inside docker exit code
exit "$?"
