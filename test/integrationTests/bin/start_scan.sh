#!/bin/sh

cd `dirname $0`

docker exec \
    modules-poz-1 /bin/bash -o pipefail -c "
        caput poz:trigger.DISA 0 ;
        exit "'$?'"
    "

# return last exit code = docker exit code = last command inside docker exit code
exit "$?"
