#!/bin/bash

# NOTE: This script contains CODAC specific tools (e.g. softIOC).

function usage {
cat <<EOM
usage: $0 [-hl] [-p poz_ip] [-x xpoz_ip] [-w hw_address] [-u min_update_period] [-f polled_fields_update_period] [-b heartbeat_period] [-m rate_limit] [-n max_records_per_poz_ioc] [-r records] [-s samples] [-t time] [-a max_camonitor_args] [-c monitor_update_tool_pvs] [-d bash_terminal_camonitor_pvs]
-h display help (this)
-l calculate latencies (only with -d option)
-p poz_ip specify poz hostname or IP address (default diode-int)
-x xpoz_ip specify xpoz hostname or IP address (default $HOSTNAME)
-w hw_address specify diode sender destination address (default xpoz_ip)
-u min_update_period specify minimum update period in seconds as a float
-f polled_fields_update_period specify polled fields update period in seconds as a float
-b heartbeat_period specify heartbeat period in seconds as a float
-m rate_limit specify rate limit in MB/s as a float
-n max_records_per_poz_ioc specify maximum number of records per POZ IOC as an integer (default 0: no limit)
-r records specify number of records (PVs) in test (default 1000)
-s samples specify number of times each PV should process (default 100)
-t time specify scanning period time in milliseconds (default 100)
-a max_camonitor_args specify maximum number camonitor arguments (only with -d option) (default 1000)
-c monitor_update_tool_pvs specify number of PVs to monitor with Diode monitor_update tool (default all)
-d bash_terminal_camonitor_pvs specify number of PVs to monitor with bash terminal camonitor (default 0)
EOM
}

function failexit { echo "$@"; exit 1; }

#FUNCTION TO KILL ANY DETACHED PROCESSES WE START AS SCREENS
function killscreens {
	screen -ls | awk -vFS='\t|[.]' '/camonitors/ {system("screen -S "$2" -X quit")}'
	screen -S monitor-updates -X quit >/dev/null 2>&1
	screen -S xpoz-ioc -X quit >/dev/null 2>&1
	ssh -t ${POZIP} "
		screen -S diode-sender -X quit
		screen -ls | awk -vFS='\t|[.]' '/poz-ioc/ {system(\"screen -S \"\$2\" -X quit\")}'
	" > /dev/null 2>&1
}
trap killscreens EXIT

#SET PATHS
DIODE_ROOT=/opt/codac/apps/epics-diode
TMP_DIR=/tmp/diode-test
[[ -s ${DIODE_ROOT}/cfg/diode.json ]] || failexit "diode.json not found in ${DIODE_ROOT}/cfg/diode.json! Exiting."

#DEFAULT PARAMETERS, MAY BE OVERWRITTEN BY OPTS
POZIP=diode-int
XPOZIP="${HOSTNAME}"
HWADDR="${HOSTNAME}"
min_update_period="$(grep "min_update_period"                     ${DIODE_ROOT}/cfg/diode.json | grep -o [0-9\.]*)"
polled_fields_update_period="$(grep "polled_fields_update_period" ${DIODE_ROOT}/cfg/diode.json | grep -o [0-9\.]*)"
heartbeat_period="$(grep "heartbeat_period"                       ${DIODE_ROOT}/cfg/diode.json | grep -o [0-9\.]*)"
rate_limit_mbs="$(grep "rate_limit_mbs"                           ${DIODE_ROOT}/cfg/diode.json | grep -o [0-9\.]*)"
POZIOCSIZE=0
RECORDS=1000
SAMPLES=100
PERIODMS=100
PERIOD=".1 second"
MAXCAMONITORARGS=1000
BTMONITORPVS=0

while getopts "hlp:x:w:u:f:b:m:n:r:s:t:a:c:d:" opt; do
case ${opt} in
	h)
		usage
		exit 2
	;;
	l)
		CALCLATENCY=1
	;;
	p)
		POZIP="${OPTARG}"
	;;
	x)
		XPOZIP="${OPTARG}"
	;;
	w)
		HWADDR="${OPTARG}"
	;;
	u)
		min_update_period="${OPTARG}"
		echo "$min_update_period>=0.025" | bc -l | grep -q 1 ||
			failexit "Minimum update period must be a real number greater than or equal to 0.025. Exiting."
	;;
	f)
		polled_fields_update_period="${OPTARG}"
		echo "$polled_fields_update_period>=3.0" | bc -l | grep -q 1 ||
			failexit "Polled field update period must be a real number greater than or equal to 3.0. Exiting."
	;;
	b)
		heartbeat_period="${OPTARG}"
		echo "$heartbeat_period>=0.1" | bc -l | grep -q 1 ||
			failexit "Heartbeat Period must be a real number greater than or equal to 0.1. Exiting."
	;;
	m)
		rate_limit_mbs="${OPTARG}"
		echo "$rate_limit_mbs>=0" | bc -l | grep -q 1 ||
			failexit "Rate limit must be a real number greater than or equal to 0. Exiting."
	;;
	n)
		POZIOCSIZE="${OPTARG}"
		[[ "$POZIOCSIZE" =~ ^[0-9]+$ ]] ||
			failexit "Maximum records per POZ IOC must be an integer greater than or equal to 0. Exiting."
	;;
	r)
		RECORDS="${OPTARG}"
		[[ "$RECORDS" =~ ^[0-9]+$ && "$RECORDS" -gt 0 ]] ||
			failexit "Number of records must be an integer greater than or equal to 1. Exiting."
	;;
	s)
		SAMPLES="${OPTARG}"
		[[ "$SAMPLES" =~ ^[0-9]+$ && "$SAMPLES" -gt 0 ]] ||
			failexit "Number of samples must be an integer greater than or equal to 1. Exiting."
	;;
	t)
		PERIODMS="${OPTARG}"
		case "${PERIODMS}" in
			"10000") PERIOD="10 second" ;;
			"5000")  PERIOD="5 second"  ;;
			"2000")  PERIOD="2 second"  ;;
			"1000")  PERIOD="1 second"  ;;
			"500")   PERIOD=".5 second" ;;
			"200")   PERIOD=".2 second" ;;
			"100")   PERIOD=".1 second" ;;
			*) failexit "Scanning period must be one of the following values: 10000, 5000, 2000, 1000, 500, 200, 100. Input value \"${OPTARG}\" is invalid. Exiting." ;;
		esac
	;;
	a)
		MAXCAMONITORARGS="${OPTARG}"
		[[ "$MAXCAMONITORARGS" =~ ^[0-9]+$ && "$MAXCAMONITORARGS" -gt 0 ]] ||
			failexit "Maximum number of arguments for camonitors must be an integer greater than or equal to 1. Exiting."
	;;
	c)
		MUMONITORPVS="${OPTARG}"
		[[ "$MUMONITORPVS" =~ ^[0-9]+$ ]] ||
			failexit "Number of PVs must be an integer greater than or equal to 0. Exiting."
	;;
	d)
		BTMONITORPVS="${OPTARG}"
		[[ "$BTMONITORPVS" =~ ^[0-9]+$ ]] ||
			failexit "Number of PVs must be an integer greater than or equal to 0. Exiting."
	;;
	:)
		echo "Option -${OPTARG} requires an argument."
		usage
		exit 1
	;;
	?)
		echo "Invalid option: -${OPTARG}."
		usage
		exit 1
	;;
esac
done

#IN CASE IT IS NOT SET BY OPTION
MUMONITORPVS=${MUMONITORPVS:-${RECORDS}}
[[ ${MUMONITORPVS} -gt ${RECORDS} || ${BTMONITORPVS} -gt ${RECORDS} ]] &&
	failexit "Number of PVs to monitor may not be greater than number of records. Exiting."

#SYSTEM CHECKS TO ENSURE EVERYTHING IS SET UP CORRECTLY
ssh -q ${POZIP} "[[ -f ${DIODE_ROOT}/bin/diode_sender ]] && { rm -rf ${TMP_DIR}; mkdir -p ${TMP_DIR}; }"
RETURN=$?
[[ $RETURN -eq 1 ]] && failexit "Diode sender not found on POZ remote host \"${POZIP}\". Exiting."
[[ $RETURN -ne 0 ]] && failexit "Could not connect to POZ remote host \"${POZIP}\". Exiting."
if [[ "${HWADDR}" != "${XPOZIP}" ]]; then
	ssh "$HWADDR" "systemctl is-active --quiet wfstrmtx" || failexit "Service wfstrmtx is not running on \"${HWADDR}\". Exiting."
fi
read default max < <(ssh ${POZIP} "echo \$(sysctl -n net.core.wmem_default) \$(sysctl -n net.core.wmem_max)")
[[ "$default" -lt 1000000 ]] &&
	failexit "sysctl net.core.wmem_default on POZ is $default. Must be at least 1000000! Exiting."
[[ "$max"     -lt 1000000 ]] &&
	failexit "sysctl net.core.wmem_max on POZ is $max. Must be at least 1000000! Exiting."
[[ "${HOSTNAME}" != "${XPOZIP}"* ]] && ! grep -q "${XPOZIP}" <<< $(ifconfig) &&
	failexit "XPOZ hostname/IP ${XPOZIP} invalid. Exiting."
[[ "$(hostname -I)" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+ ]] ||
	failexit "Could not determine on which XPOZ network interface to listen. Exiting."
command -v camonitor >/dev/null 2>&1 ||
	failexit "camonitor command not found! Exiting."
[[ -f ${DIODE_ROOT}/bin/testDiode ]] ||
	failexit "testDiode not found in ${DIODE_ROOT}/bin/testDiode! Exiting."
[[ -f ${DIODE_ROOT}/cfg/readOnly.acf ]] ||
	failexit "readOnly.acf not found in ${DIODE_ROOT}/cfg/readOnly.acf! Exiting."
[[ -f ${DIODE_ROOT}/dbd/testDiode.dbd ]] ||
	failexit "testDiode.dbd not found in ${DIODE_ROOT}/dbd/testDiode.dbd! Exiting."
[[ ${MUMONITORPVS} -eq 0 || -f ${DIODE_ROOT}/bin/monitor_updates ]] ||
	failexit "monitor_updates not found in ${DIODE_ROOT}/bin/monitor_updates! Exiting."
[[ "$(sysctl -n net.core.rmem_default)" -lt 1000000 ]] &&
	failexit "sysctl net.core.rmem_default on XPOZ must be at least 1000000! Exiting."
[[ "$(sysctl -n net.core.rmem_max)" -lt 1000000 ]] &&
	failexit "sysctl net.core.rmem_max on XPOZ must be at least 1000000! Exiting."
[[ "$(tuned-adm profile | grep 'Current active profile' | awk '{print $4}')" != "network-latency" ]] &&
	echo "Warning! tuned-adm profile is not set to \"network-latency\". This may impact performance."
systemctl status irqbalance > /dev/null 2>&1
[[ "$?" -eq 0 ]] && echo "Warning. irqbalance service is running which may impact performance."

#INFORM USER AND CREATE LOG FILE
TEST_TIME="$(date +%Y-%m-%d_%H:%M:%S)"
echo "Running Diode performance test with the following parameters:
POZ hostname/IP:                        ${POZIP}
XPOZ hostname/IP:                       ${XPOZIP}
Diode sender destination address:       ${HWADDR}
Number of records:                      ${RECORDS}
Number of samples per record:           ${SAMPLES}
Total number of samples:                $((${RECORDS}*${SAMPLES}))
Scanning time period:                   ${PERIODMS} ms (${PERIOD})
Maximum records per POZ IOC:            $([[ "${POZIOCSIZE}" -eq 0 ]] && echo "No limit" || echo "${POZIOCSIZE}")
Number of POZ IOCs:                     $([[ "${POZIOCSIZE}" -eq 0 ]] && echo "1" || echo $((((${RECORDS}-1)/${POZIOCSIZE})+1)))
Diode minimum update period:            ${min_update_period} s
Diode polled fields update period:      ${polled_fields_update_period} s
Diode heartbeat period:                 ${heartbeat_period} s
Diode rate limit:                       ${rate_limit_mbs} MB/s
Diode monitor_update PVs to monitor:    ${MUMONITORPVS}
Bash terminal camonitor PVs to monitor: ${BTMONITORPVS}
Maximum number of camonitor arguments:  ${MAXCAMONITORARGS}
Calculate latencies:                    $([[ -n ${CALCLATENCY} ]] && echo "Yes" || echo "No")" | tee DPT-${TEST_TIME}.log
printf %.1s \#{1..40} $'\n' >> DPT-${TEST_TIME}.log

#STATUS UPDATE
echo -n "Setting up test environment and necessary files... "

#STOP ANY OLD SCREENS WHICH MAY HAVE SURVIVED PREVIOUS TESTS
killscreens

#CREATE LOCAL XPOZ WORKDIR
rm -rf ${TMP_DIR}
mkdir -p ${TMP_DIR}

#GENERATE diode.json based on /opt/codac/apps/epics-diode/cfg/diode.json
echo "{
	\"min_update_period\": ${min_update_period},
	\"polled_fields_update_period\": ${polled_fields_update_period},
	\"heartbeat_period\": ${heartbeat_period},
	\"rate_limit_mbs\": ${rate_limit_mbs},
	\"channel_names\": {" > ${TMP_DIR}/diode.json
awk -v records=${RECORDS} 'BEGIN {
	for (record=1; record<records; record++)
		print "		\"poz:pv" record "\": {},"
	}' >> ${TMP_DIR}/diode.json
echo -e "\t\t\"poz:pv${RECORDS}\": {}\n\t}\n}" >> ${TMP_DIR}/diode.json

#GENERATE diode_ioc.cmd based on /opt/codac/apps/epics-diode/bin/diode_ioc.cmd
echo "#!${DIODE_ROOT}/bin/testDiode
asSetFilename(\"${DIODE_ROOT}/cfg/readOnly.acf\")
dbLoadDatabase(\"${DIODE_ROOT}/dbd/testDiode.dbd\")
testDiode_registerRecordDeviceDriver(pdbbase)
diodeLogLevel(5)
dbLoadRecords(\"${TMP_DIR}/xpoz.db\")
diodeReceiverStart(\"${TMP_DIR}/diode.json\", 5080, \"${XPOZIP}\")
diodeIocInit()" > ${TMP_DIR}/diode_ioc.cmd
chmod +x ${TMP_DIR}/diode_ioc.cmd

#GENERATE POZ DBs
[[ "${POZIOCSIZE}" -eq 0 ]] && POZIOCSIZE=${RECORDS}
REMAINDER=${RECORDS}
BATCH=0
while [[ ${REMAINDER} -gt 0 ]]; do
	((BATCH++))
	FIRST=$((($BATCH-1)*${POZIOCSIZE}+1))
	LAST=$((${REMAINDER}>${POZIOCSIZE}?${FIRST}+${POZIOCSIZE}-1:${FIRST}+${REMAINDER}-1))
	REMAINDER=$((${REMAINDER}+${FIRST}-${LAST}-1))
	echo -e "record(event,\"poz${BATCH}:event\") {\n\tfield(VAL, \"${BATCH}\")\n}" > ${TMP_DIR}/poz${BATCH}.db
	awk -v first=${FIRST} -v last=${LAST} -v samples=${SAMPLES} -v batch=${BATCH} 'BEGIN {
		for (record=first; record<=last; record++)
			print "record(calc,\"poz:pv" \
				record \
				"\") {\n\tfield(INPA, \"" \
				samples \
				"\")\n\tfield(INPB, \"poz:pv" \
				record \
				"\")\n\tfield(CALC, \"B<A?B+1:B\")\n\tfield(SCAN, \"Event\")\n\tfield(EVNT, \"" \
				batch \
				"\")\n}"
	}' >> ${TMP_DIR}/poz${BATCH}.db
done

#GENERATE XPOZ DB
awk -v records=${RECORDS} 'BEGIN {for (record=0; record<records; record++)
	print "record(ai,\"xpoz:pv" record+1 "\") {\n\tinfo(diode_cix, \"" record "\")\n}"
}' > ${TMP_DIR}/xpoz.db

#BUILD EXPECTED OUTPUT FILE WITH JUST PVS AND EXPECTED VALUES (NO TIMESTAMPS OR ALARMS)
if [[ ${BTMONITORPVS} -gt 0 ]]; then
	[[ $((${BTMONITORPVS}*${SAMPLES})) -gt 500000 ]] &&
		{ echo ; read -p "WARNING! You are about to generate an output file that is $((${BTMONITORPVS}*${SAMPLES})) lines long. Are you sure you want to proceed?"; }
	awk "BEGIN {
		for (record=1; record<=${BTMONITORPVS}; record++)
		for (sample=1; sample<=${SAMPLES}; sample++)
			print \"xpoz:pv\" record, sample
	}" > ${TMP_DIR}/expected-output
fi

#STATUS UPDATE
echo -en "Done!\nDeploying remote processes on POZ (POZ IOCs and Diode Sender)... "

#START REMOTE PROCESSES ON POZ
scp ${TMP_DIR}/diode.json ${TMP_DIR}/poz*.db ${POZIP}:${TMP_DIR}/ > /dev/null
ssh ${POZIP} "
	for db in ${TMP_DIR}/poz*.db; do
		screen -dmS poz-ioc\${db//[!0-9]/} softIoc -d \${db}
	done;
	screen -dmS diode-sender sh -c \"${DIODE_ROOT}/bin/diode_sender -c ${TMP_DIR}/diode.json ${HWADDR}\"
"

#STATUS UPDATE
echo -en "Done!\nDeploying local processes on XPOZ (XPOZ Receiver, CA Monitors and monitor_updates)... "

#START LOCAL PROCESSES ON XPOZ AND WAIT FOR MONITORS TO CONNECT (SPLIT CAMONITORS BY BATCHES TO AVOID EXCEEDING MAXIMUM SHELL ARGUMENTS)
screen -dmS xpoz-ioc ${TMP_DIR}/diode_ioc.cmd
if [[ ${BTMONITORPVS} -gt 0 ]]; then
	REMAINDER=${RECORDS}
	BATCH=0
	while [[ ${REMAINDER} -gt 0 ]]; do
		((BATCH++))
		FIRST=$((($BATCH-1)*${MAXCAMONITORARGS}+1))
		LAST=$((${REMAINDER}>${MAXCAMONITORARGS}?${FIRST}+${MAXCAMONITORARGS}-1:${FIRST}+${REMAINDER}-1))
		REMAINDER=$((${REMAINDER}+${FIRST}-${LAST}-1))
		screen -dmS camonitors${BATCH} sh -c "
			camonitor -tsc -F~ $(printf "xpoz:pv%i " $(seq ${FIRST} ${LAST})) > ${TMP_DIR}/output${BATCH}
		"
	done
	for ((i=1; i<=${BATCH}; i++)); do
		while ! [[ "$(tail -1 ${TMP_DIR}/output${i} 2>/dev/null)" = *"~0~UDF~INVALID" ]] ; do
			sleep 1
		done
	done
fi
if [[ ${MUMONITORPVS} -gt 0 ]]; then
	screen -dmS monitor-updates sh -c "stdbuf -oL -eL ${DIODE_ROOT}/bin/monitor_updates xpoz:pv ${MUMONITORPVS} 0 > ${TMP_DIR}/monitor_updates"
	while ! [[ "$(tail -1 ${TMP_DIR}/monitor_updates 2>/dev/null)" = *"No updates received in the last second"* ]] ; do
		sleep 1
	done
fi

#ASSIGN XPOZ DIODE RECEIVER AFFINITY TO SECOND TO LAST CORE AND CA SERVER TO LAST CORE
CORES=$(grep -m 1 'cpu cores' /proc/cpuinfo | grep -o [0-9]*)
CPUS_CORE_A=($(lscpu --all --extended=CPU,CORE | tail -n+2 | awk "{if (\$2==$((${CORES}-1))) print \$1}"))
CPUS_CORE_B=($(lscpu --all --extended=CPU,CORE | tail -n+2 | awk "{if (\$2==$((${CORES}-2))) print \$1}"))
printf -v CPUSTRING '%s,' ${CPUS_CORE_A[@]} ${CPUS_CORE_B[@]}
if [[ "${CORES}" -gt 2 ]]; then
	sudo tuna -c ${CPUSTRING%,} -i
	taskset -p -c ${CPUS_CORE_A[0]} $(ps -To tid,comm -p $(pgrep diode_ioc.cmd) | grep "diode receiver" | awk '{print $1}') > /dev/null
	taskset -p -c ${CPUS_CORE_B[0]} $(ps -To tid,comm -p $(pgrep diode_ioc.cmd) | grep CAS-TCP | awk '{print $1}') > /dev/null
	taskset -p -c ${CPUS_CORE_B[0]} $(ps -To tid,comm -p $(pgrep diode_ioc.cmd) | grep CAS-UDP | awk '{print $1}') > /dev/null
else
	echo -e "\nUnable to set core affinities to XPOZ threads."
fi

#RAISE PRIORITY OF XPOZ IOC DIODE RECEIVER THREAD TO 80
sudo chrt -f -p 80 $(ps -To tid,comm -p $(pgrep diode_ioc.cmd) | grep "diode receiver" | awk '{print $1}')

#STATUS UPDATE
echo -en "Done!\nData transmission triggered ($(((${PERIODMS}*${SAMPLES}/1000))) seconds)... "

#START TRIGGERS
for db in ${TMP_DIR}/poz*.db; do
	caput poz${db//[!0-9]/}:event.SCAN "${PERIOD}" > /dev/null
done

#WAIT CALCULATED TIME PLUS 2 SECONDS FOR MARGIN
sleep $(((${PERIODMS}*${SAMPLES}/1000)+2))

#STOP ALL DETACHED PROCESSES RUNNING ON SCREENS
killscreens

#STATUS UPDATE
if [[ -n ${CALCLATENCY} ]]; then
	echo -e "Done!\nAnalyzing data post-mortem (~$((${RECORDS}*${SAMPLES}/32000)) minutes)..."
else
	echo -e "Done!\nAnalyzing data post-mortem... "
fi

#ANALYZE MONITOR_UPDATES OUTPUT FOR ANY LOSSES IF WE ARE USING MONITOR_UPDATES
if [[ ${MUMONITORPVS} -gt 0 ]]; then
	MISSED_UPDATES=$(grep -o 'Missed updates\/sec: [0-9]*' ${TMP_DIR}/monitor_updates |
		grep -o '[0-9]*' | awk '{sum+=$1} END {print sum}')
	[[ -z $MISSED_UPDATES ]] && MISSED_UPDATES=0
	RECEIVED_UPDATES=$(grep -o 'Received updates\/sec: [0-9]*' ${TMP_DIR}/monitor_updates |
		grep -o '[0-9]*' | awk '{sum+=$1} END {print sum}')
	[[ -z $RECEIVED_UPDATES ]] && RECEIVED_UPDATES=0
	echo "Missing samples using monitor_updates: ${MISSED_UPDATES}/$((${SAMPLES}*${MUMONITORPVS}))" \
		"($(echo "scale=10; 100*${MISSED_UPDATES}/$((${SAMPLES}*${MUMONITORPVS}))" | bc)%)" |
		tee -a DPT-${TEST_TIME}.log
	echo "Received samples using monitor_updates: ${RECEIVED_UPDATES}/$((${SAMPLES}*${MUMONITORPVS}))" \
		"($(echo "scale=10; 100*${RECEIVED_UPDATES}/$((${SAMPLES}*${MUMONITORPVS}))" | bc)%)" |
		tee -a DPT-${TEST_TIME}.log
fi

#ANALYZE CAMONITOR OUTPUT VS EXPECTED OUTPUT IF WE ARE USING CAMONITORS
if [[ ${BTMONITORPVS} -gt 0 ]]; then

	#MERGE ALL OUTPUT FILES AND CLEAN UP (REMOVE DISCONNECTS AND INITIAL 0 UDF VALUES, REPLACE CLIENT TIMESTAMP DELIMITER)
	cat ${TMP_DIR}/output* > ${TMP_DIR}/output
	sed -i "/\*\*\*/d;/.*~.*~0~.*~.*/d;s/[()]/~/g" ${TMP_DIR}/output

	#GENERATE LATENCIES FILE AND SORT FROM MIN TO MAX
	if [[ -n ${CALCLATENCY} ]]; then
		paste <(cut -d~ -f2 ${TMP_DIR}/output | xargs -I{} date -d "{}" +%s.%N) \
		      <(cut -d~ -f3 ${TMP_DIR}/output | xargs -I{} date -d "{}" +%s.%N) |
		      awk '{printf "%.6f\n", $2 - $1}' > ${TMP_DIR}/latencies
		[[ -s ${TMP_DIR}/latencies ]] && sort -on ${TMP_DIR}/latencies ${TMP_DIR}/latencies
	fi

	#STRIP OUTPUT TO JUST PVS AND VALUES RECEIVED (NO TIMESTAMPS OR ALARMS)
	awk -i inplace -F~ '{print $1, $5}' ${TMP_DIR}/output

	#COMPARE EXPECTED VS ACTUAL AND FIND WHICH ARE MISSING
	sort -o ${TMP_DIR}/output ${TMP_DIR}/output
	sort -o ${TMP_DIR}/expected-output ${TMP_DIR}/expected-output
	MISSING_SAMPLES=$(comm -23 ${TMP_DIR}/expected-output ${TMP_DIR}/output | wc -l)

	#PRINT RESULTS AND GENERATE LOG FILE
	echo "Missing samples using camonitor: ${MISSING_SAMPLES}/$((${SAMPLES}*${BTMONITORPVS}))" \
		"($(echo "scale=10; 100*${MISSING_SAMPLES}/$((${SAMPLES}*${BTMONITORPVS}))" | bc)%)" |
		tee -a DPT-${TEST_TIME}.log
	if [[ -s ${TMP_DIR}/latencies ]]; then
		echo "Minumum latency = $(head -1 ${TMP_DIR}/latencies)" | tee -a DPT-${TEST_TIME}.log
		echo "Maximum latency = $(tail -1 ${TMP_DIR}/latencies)" | tee -a DPT-${TEST_TIME}.log
	fi
fi

#CLEAN UP
ssh ${POZIP} "rm -rf ${TMP_DIR}"
rm -rf ${TMP_DIR}
