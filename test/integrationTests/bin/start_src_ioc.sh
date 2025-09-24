#!/bin/sh

cd `dirname $0`

docker exec \
    modules-poz-1 /bin/bash -o pipefail -c "
        cd /test_bin/sourceIoc/iocBoot/iocsource &&
        screen -S ioc -d -m ../../bin/linux-x86_64/source ./st.cmd &&
        sleep 15;
        exit "'$?'"
    "

# return last exit code = docker exit code = last command inside docker exit code
exit "$?"
