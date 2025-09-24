#!/bin/sh

cd `dirname $0`

# Remove test DB folders.
sudo rm -rf \
    "$TEST_VOLUMES_FOLDER/archivedb" \
    "$TEST_VOLUMES_FOLDER/bin/sourceIoc/bin" \
    "$TEST_VOLUMES_FOLDER/bin/sourceIoc/dbd" \
    "$TEST_VOLUMES_FOLDER/bin/sourceIoc/db" \
    "$TEST_VOLUMES_FOLDER/bin/sourceIoc/lib" \
    "$TEST_VOLUMES_FOLDER/output/monitor.out" \
    "$TEST_VOLUMES_FOLDER/output/success" \
    "$TEST_VOLUMES_FOLDER/output/failed"

# Remove test log folders
if [ -x "$TEST_VOLUMES_FOLDER"/log ]; then
	rm -rf "$TEST_VOLUMES_FOLDER"/log/*
fi

