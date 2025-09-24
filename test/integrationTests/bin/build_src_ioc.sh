#!/bin/sh

cd `dirname $0`

docker exec \
    modules-poz-1 /bin/bash -o pipefail -c "
        cd /test_bin/sourceIoc &&
        make distclean &&
        make &&
        sleep 5 &&
        exit "'$?'"
    "

# return last exit code = docker exit code = last command inside docker exit code
exit "$?"
