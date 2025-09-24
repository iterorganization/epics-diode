#!/bin/sh

# Add extra test file as needed
cd "$TEST_OUTPUT_FOLDER" && \
"$TESTFOLDER"/analyze.py monitor.out && \
touch success && \
exit 0

touch failed
exit 1

