#!/bin/sh

cd `dirname $0`

RUNFILE=.running

if [ ! $1 ]; then
    if [ -e "$RUNFILE" ]; then
        exit 0
    fi
    exit 1
fi

RUNMODE=$1
if [ "$RUNMODE" = "test" ]; then
    if [ -e "$RUNFILE" ]; then
        exit 0
    fi
    exit 1
elif [ "$RUNMODE" = "empty" ]; then
    if [ -e "$RUNFILE" ]; then
        exit 1
    fi
    exit 0
fi

exit 1
