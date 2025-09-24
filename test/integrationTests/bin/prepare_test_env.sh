#!/bin/sh

#
# General environment settings for running commands
#
# 
# Exports:
#
# BINFOLDER            = Full path location of scripts.
# TESTNAME             = Folder and test name.
# TESTFOLDER           = Full path location of a particular test.
# TESTSFOLDER          = Full path location folder of all tests.
# VOLUMES_FOLDER       = Full path of volumes folder.
# TEST_VOLUMES_FOLDER  = Full path of test volumes folder.
# TEST_BIN_FOLDER      = Full path of test volumes/bin folder.
# TEST_OUTPUT_FOLDER   = Full path of test volumes/output folder.
# DIODE_BIN_FILDER     = Full path to the epics-diode bin folder
#


# cd `dirname $0`

FOLDER="../volumes/test"
VOLUMES="../volumes"
MODULES="../modules"

export BINFOLDER=`pwd`
export DIODE_BIN_FOLDER="../../../bin/$EPICS_HOST_ARCH"
export DIODE_ROOT_FOLDER="../../.."

cd $FOLDER
export TESTSFOLDER=`pwd`

if [ ! $1 ]; then
    return
fi

export TESTNAME=$1

if [ ! -x $1 ]; then
    TEST_DOES_NOT_EXISTS=1
    return
fi

cd $TESTNAME
export TESTFOLDER=`pwd`

cd ../..
export VOLUMES_FOLDER=`pwd`
export COMPOSE_FILE_YML="$MODULES/test.yml"
export TEST_VOLUMES_FOLDER="$TESTFOLDER/volumes"
export TEST_BIN_FOLDER="$TEST_VOLUMES_FOLDER/bin"
export TEST_OUTPUT_FOLDER="$TEST_VOLUMES_FOLDER/output"




