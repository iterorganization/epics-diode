EPICS Diode Tools
==================

This module provides the following set of tools:

- `diode_sender`_ - EPICS CA Diode sender
- `diode_receiver`_ - EPICS CA Diode receiver that dumps new values to stdout (for debugging)
- `diode_dbgen`_ - a tool that generates receiver-side IOC .db by querying sender-side PVs
- `IOC shell integration`_ - EPICS CA Diode receiver, device and record support

- `pvadiode_sender`_ - EPICS pvAccess Diode sender
- `pvadiode_receiver`_ - EPICS pvAccess Diode receiver


diode_sender
------------
A stand-alone `EPICS CA Diode` sender process - a CA client subscribing to a predefined list of channels and pushing changes to the transport layer. 

.. code-block:: shell

    $ ./bin/linux-x86_64/diode_sender -d -c test/testDiodeApp/src/test_diode.json localhost
    2023-02-25T09:17:09.371 [config] Loading configuration from 'test/testDiodeApp/src/test_diode.json'.
    2023-02-25T09:17:09.371 [sender] Initializing transport, send list: [127.0.0.1:5080].
    2023-02-25T09:17:09.371 [sender] Send rate-limit set to 64MB/s.
    2023-02-25T09:17:09.371 [sender] Update period 0.100s, heartbeat period 15.0s.
    2023-02-25T09:17:09.371 [sender] Initializing CA.
    2023-02-25T09:17:09.371 [sender] Creating 8 channels.    

diode_receiver
--------------
A development (debugging) version of `EPICS CA Diode` receiver - all updates are printed to standard output.

.. code-block:: shell

    $ ./bin/linux-x86_64/diode_receiver -c test/testDiodeApp/src/test_diode.json
    2023-02-25T09:20:39.258 [config] Loading configuration from 'test/testDiodeApp/src/test_diode.json'.
    2023-02-25T09:20:39.261 [receiver] Initializing transport, listening at '*:5080'.
    2023-02-25T09:20:39.261 [receiver] Creating 8 channels.
    [                         poz:ai1] DBR_TIME_DOUBLE       3  2   TimeStamp: 2023/01/05 09:20:40.361583   Value: 8.0000 
    [                         poz:ai2] DBR_TIME_DOUBLE       6  1   TimeStamp: 2023/01/05 09:20:40.361583   Value: 4.0000 

diode_dbgen
-----------
A tool that generates external IOC .db out for each channel in the configuration file. The tool uses CA to query for all the metadata.
Note that record names will match the ones in the configuration file.

.. code-block:: shell

    $ ./bin/linux-x86_64/diode_dbgen -c test/testDiodeApp/src/test_diode.json 
    2023-02-25T09:22:55.248 [config] Loading configuration from 'test/testDiodeApp/src/test_diode.json'.
    2023-02-25T09:22:55.248 [diode_dbgen] Initializing CA.
    2023-02-25T09:22:55.248 [diode_dbgen] Processing 1/8: 'poz:ai1'.
    record(ai, "poz:ai1")
    {
        info(diode_cix, "0")
        field(EGU,  "Counts")
        field(HIHI, "8")
        field(HIGH, "6")
        field(LOW , "4")
        field(LOLO, "2")
        field(HOPR, "10")
        field(LOPR, "0")
    }
    ...

IOC shell integration
---------------------
IOC shell integration is provided, mainly to be able to start IOC-embedded `EPICS Diode` receiver
together with EPICS device and record support (``diodeIoc`` shared-library).

.. code-block:: shell

    # make sure to include "diode.dbd"
    #include "diode.dbd"

    # Set epics-diode logging level.
    # Trace = 0, Debug = 1, Config = 2, Info = 3 (default), Warning = 4, Error = 5 
    diodeLogLevel(2)

    # Start epics-diode sender (to be used for testing only, use standalone sender instead).
    #   parameters: <configuration filename> <sender address>
    # NOTE: MUST NOT be started after IOC CA initialization (i.e. after IOC installs local CA service).
    #diodeSenderStart("cfg/test_diode.json", "localhost:5080")

    # Start epics-diode receiver.
    #   parameters: <configuration filename> <listening port> <listening IP>
    diodeReceiverStart("cfg/test_diode.json", 5080, "0.0.0.0")

    # Start epics-diode engine.
    diodeIocInit()

pvadiode_sender
------------
A stand-alone `EPICS PVA Diode` sender process - a PVA client subscribing to a predefined list of channels and pushing changes to the transport layer. 

.. code-block:: shell

    $ ./bin/linux-x86_64/pvadiode_sender -d -c diode.json localhost
    2025-04-09T15:15:19.608 [config] Loading configuration from 'diode.json'.
    2025-04-09T15:15:19.608 [pva.sender] Initializing transport, send list: [127.0.0.1:5081].
    2025-04-09T15:15:19.608 [pva.sender] Send rate-limit set to 125MB/s.
    2025-04-09T15:15:19.608 [pva.sender] Update period 0.025s, heartbeat period 30.0s.
    2025-04-09T15:15:19.608 [pva.sender] Initializing PVA.
    2025-04-09T15:15:19.610 [pva.sender] Creating 1 channels.    

pvadiode_receiver
--------------
A stand-alone `EPICS PVA Diode` receiver process - a PVA server that exposes a predefined set of channels and listens for updates from the transport layer. 

.. code-block:: shell

    $ ./bin/linux-x86_64/pvadiode_receiver -c diode.json
    2025-04-09T15:19:32.188 [config] Loading configuration from 'diode.json'.
    2025-04-09T15:19:32.200 [pva.receiver] Initializing transport, listening at '0.0.0.0:5081'.
    2025-04-09T15:19:32.200 [pva.receiver] Creating 1 channels.

