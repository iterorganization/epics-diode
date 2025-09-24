#!/bin/sh

# 
# Check 'prepare_test_env.sh' for exported vars.
#

print_usage() {
    echo "USAGE: $0 <TEST_NAME> <COMMAND>"
    echo "Commands: clean, up, run, stop, down, report, all"
    echo "Tests:"
#    echo $TESTSFOLDER
    sudo find $TESTSFOLDER -name .test | grep -v .template | xargs -Ixx dirname xx | xargs -Ixx basename xx | sort -u | xargs -Ixx echo -n "xx "
    echo ""
}

test_clean() {
    _RET=0
    "$BINFOLDER"/check_run.sh empty && \
    "$BINFOLDER"/remove_test_volumes.sh && \
    _RET=1
    return $_RET
}

test_up() {
    "$BINFOLDER"/check_run.sh empty && \
    if [ -x "up.sh" ]; then
        _RET=0
        if [ -e "manual_monitor" ]; then
            echo "test $TESTNAME" > "$BINFOLDER"/.running && \
            ./up.sh && \
            "$BINFOLDER"/build_src_ioc.sh && \
            "$BINFOLDER"/start_src_ioc.sh && \
            "$BINFOLDER"/build_dst_db.sh && \
            "$BINFOLDER"/start_dst_ioc.sh && \
            "$BINFOLDER"/start_dst_monitor_manual.sh && \
            "$BINFOLDER"/start_diode_sender.sh && \
            _RET=1
            return $_RET
        else
            echo "test $TESTNAME" > "$BINFOLDER"/.running && \
            ./up.sh && \
            "$BINFOLDER"/build_src_ioc.sh && \
            "$BINFOLDER"/start_src_ioc.sh && \
            "$BINFOLDER"/build_dst_db.sh && \
            "$BINFOLDER"/start_dst_ioc.sh && \
            "$BINFOLDER"/start_dst_monitor.sh && \
            "$BINFOLDER"/start_diode_sender.sh && \
            _RET=1
            return $_RET
        fi
    else
        echo "$TESTFOLDER/up.sh missing!"
        exit 1
    fi
}

test_run() {
    "$BINFOLDER"/check_run.sh test $TESTNAME && \
    if [ -x "run.sh" ]; then
        _RET=0
        ./run.sh && \
        "$BINFOLDER"/start_scan.sh && \
        sleep 15 && \
        "$BINFOLDER"/stop_scan.sh && \
        _RET=1
        return $_RET
    else
        echo "$TESTFOLDER/run.sh missing!"
        exit 1
    fi
}

test_stop() {
    "$BINFOLDER"/check_run.sh test $TESTNAME && \
    if [ -x "stop.sh" ]; then
        _RET=0
        ./stop.sh && _RET=1
        return $_RET
    else
        echo "$TESTFOLDER/stop.sh missing!"
        exit 1
    fi
}

test_report() {
    _RET=0
    if [ -x "report.sh" ]; then
        _RET=0
        ./report.sh && \
        _RET=1
        return $_RET
    else
        echo "$TESTFOLDER/report.sh missing!"
        exit 1
    fi
}

test_down() {
    "$BINFOLDER"/check_run.sh test $TESTNAME && \
    if [ -x "down.sh" ]; then
        _RET=0
        ./down.sh && _RET=1
        rm "$BINFOLDER"/.running
        return $_RET 
    else
        echo "$TESTFOLDER/down.sh missing!"
        exit 1
    fi
}

cd `dirname $0`

TEST_DOES_NOT_EXISTS=0
. ./prepare_test_env.sh

if [ ! $1 ]; then
    print_usage
    exit 1
fi

if [ 1 -eq $TEST_DOES_NOT_EXISTS ]; then
    echo "Test '$TESTNAME' does not exists!\n"
    print_usage
    exit 1
fi


COMMAND=$2

if [ ! $2 ]; then
    print_usage
    exit 0
fi

cd "$TESTFOLDER"

if [ "$COMMAND" = "clean" ]; then
    test_clean
    if [ $? -ne 1 ]; then
        exit 1
    fi
    exit 0
elif [ "$COMMAND" = "up" ]; then
    test_up
    if [ $? -ne 1 ]; then
        exit 1
    fi
    exit 0
elif [ "$COMMAND" = "run" ]; then
    test_run
    if [ $? -ne 1 ]; then
        exit 1
    fi
    exit 0
elif [ "$COMMAND" = "stop" ]; then
    test_stop
    if [ $? -ne 1 ]; then
        exit 1
    fi
    exit 0
elif [ "$COMMAND" = "report" ]; then
    test_report
    if [ $? -ne 1 ]; then
        exit 1
    fi
    exit 0
elif [ "$COMMAND" = "down" ]; then
    test_down
    if [ $? -ne 1 ]; then
        exit 1
    fi
    exit 0
elif [ "$COMMAND" = "all" ]; then
    test_clean
    if [ $? -ne 1 ]; then
        exit 1
    fi
    test_up
    if [ $? -ne 1 ]; then
        test_down
        exit 1
    fi
    test_run
    if [ $? -ne 1 ]; then
        test_down
        exit 1
    fi
#    test_stop
    test_down
    if [ $? -ne 1 ]; then
        exit 1
    fi
    test_report
    exit 0
else
    echo "Illegal command: '$COMMAND'!\n"
    print_usage
    exit 1
fi


