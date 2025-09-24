#!/bin/sh

# Add extra test file as needed
cd "$TEST_OUTPUT_FOLDER" && \
"$TESTFOLDER"/analyze.py monitor.out && \
sudo touch success && \
exit 0

sudo touch failed
exit 1

