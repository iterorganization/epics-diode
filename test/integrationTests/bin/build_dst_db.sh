#!/bin/sh

cd `dirname $0`

docker exec \
    modules-poz-1 /bin/bash -o pipefail -c "
        cd /epics-diode/bin/linux-x86_64/ &&
        if [ ! -f /test_bin/destIoc/test.db ]; then
            ./diode_dbgen -c /test_config/diode.json /test_bin/destIoc/test.db ;
            sleep 5;
        fi ;
        exit "'$?'"
    "

# return last exit code = docker exit code = last command inside docker exit code
exit "$?"
