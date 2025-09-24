#!/bin/sh

export EPICS_CA_AUTO_ADDR_LIST="no"
export EPICS_CA_ADDR_LIST="xpoz"
export PV_LIST=`cat /test_config/pv_list`
while [ 1 ];
do
  caget $PV_LIST
  sleep 1
done

