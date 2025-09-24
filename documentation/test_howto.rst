Testing HOWTO
====================================

System Requirements
-------------------
Before building Epics-diode (running ``make`` in ``$BASE`` folder), the machine in question should meet the
dependencies defined in `Building <building.rst>`_. Those include Docker installation and docker image loaded
in local repository. The Docker image is used as the base for building `epics-diode-img` image which in turn
is used as the base for `POZ` and `XPOZ` Docker running containers.


Test Setup
----------
Epics-diode test environment is created using `Docker` containers and `shell` scripts. The containers are used
for `POZ` and `XPOZ` virtual machine simulation while the shell scripts are used for:

- Creating Docker images.
- Running the tests.
- Analyzing results.
- Cleaning up after tests.

Integration test environment can be found in ``$BASE/test/integrationTests`` with the following structure:

.. code-block:: shell

    ├── bin
    │   ├── build_dst_db.sh
    │   ├── build_image.sh
    │   ├── build_src_ioc.sh
    │   ├── check_run.sh
    │   ├── check_test_success.sh
    │   ├── clean_all_tests.sh
    │   ├── prepare_test_env.sh
    │   ├── remove_test_volumes.sh
    │   ├── run_all_tests.sh
    │   ├── run_test.sh
    │   ├── start_diode_sender.sh
    │   ├── start_docker.sh
    │   ├── start_dst_ioc.sh
    │   ├── start_dst_monitor_manual.sh
    │   ├── start_dst_monitor.sh
    │   ├── start_scan.sh
    │   ├── start_src_ioc.sh
    │   ├── stop_docker.sh
    │   └── stop_scan.sh
    ├── images
    │   └── epics-diode
    │       ├── Dockerfile
    │       └── utils
    │           ├── buildme.sh
    │           └── Makefile
    ├── Makefile
    ├── modules
    │   └── test.yml
    ├── runtests.sh
    └── volumes
        └── test
            ├── I22
            ...


Creating Docker Images
----------------------
During the ``make`` phase of Epics-diode building process the `epics-diode-img` Docker image is built from
`co3-full-devel:7.2.0` which should be pre-loaded in the local Docker repository.

`Makefile` in ``$BASE/test/integrationTests`` runs ``$BASE/test/integrationTests/bin/build_image.sh`` which
copies the already built Epics-diode code and initiates ``docker build`` to create the image.
The image is loaded in the local Docker repository and used for all test scenarios.

Running Tests
-------------

Automatic
^^^^^^^^^
The ``runtests.sh`` script is automatically run from `Jenkins` CI environment. The script runs ``bin/run_all_tests.sh``
script that runs all the tests included in the automatic process. At the end ``bin/check_test_success.sh`` script
assesses the results and returns the appropriate exit code that is propagated back through the scripts to signal
automatic tests results success to `Jenkins`.


Manual
^^^^^^
``bin/run_test.sh`` script is used for test runs.

.. code-block:: shell

    $ ./run_test.sh 
    USAGE: ./run_test.sh <TEST_NAME> <COMMAND>
    Commands: clean, up, run, stop, down, report, all
    Tests:
    I22 I23 I24 I25 I28 I33 I34 

When run without arguments, it displays possible arguments and lists all tests located in ``volumes/test`` folder.
The names of the folders within ``volumes/test`` represent names of tests.

The script should have the test name as first argument and test phase (command) as the second one.

Commands and their actions:

- `clean`: Executes ``bin/remove_test_volumes.sh`` which cleans the particular test's working folders and test results.
- `up`: Brings up the Docker containers (`POZ` and `XPOZ`), starts POZ-side `IOC` and `diode_sender`, and starts XPOZ-side
  `IOC` and `monitor` (`camonitor` or iterated `caget`).
- `run`: Triggers POZ-side `IOC` to start (changing data and producing monitoring events), sleep for 15 seconds, and
  halt POZ-side `IOC`.
- `stop`: Currently unused.
- `down`: Bring down the `POZ` and `XPOZ` Docker containers.
- `report`: Runs local (test specific) ``analyze.py`` Python script on ``monitor.out`` test results.
- `all`: Executes all of the above in correct order. This is the way tests are run automated (listed in ``bin/run_all_tests.sh``).


Creating New Tests
------------------

New test scenarios are created by copying folder structure from an existing test or the pre-prepared template test
(located in ``volumes/test/.template``) and applying the changes. A test is a subfolder of ``volumes/test``, where
its name indicates the name of the test. A test folder must contain ``.test`` file in order for the script system
to recognize it as a valid test scenario. 

An example test scenario subfolder structure (only relevant files are shown):

.. code-block:: shell

    ├── .test
    ├── analyze.py
    ├── down.sh
    ├── manual_monitor
    ├── report.sh
    ├── run.sh
    ├── stop.sh
    ├── up.sh
    └── volumes
        ├── bin
        │   ├── destIoc
        │   │   ├── screen_monitor_command.sh
        │   │   ├── test.db
        │   │   └── xpoz.cmd
        │   └── sourceIoc
        │       ├── configure
        │       ├── iocBoot
        │       │   ├── iocsource
        │       │   │   ├── envPaths
        │       │   │   ├── Makefile
        │       │   │   └── st.cmd
        │       │   └── Makefile
        │       ├── Makefile
        │       └── sourceApp
        │           ├── Db
        │           │   ├── Makefile
        │           │   ├── O.Common
        │           │   ├── O.linux-x86_64
        │           │   ├── poz.substitutions
        │           │   └── test.template
        │           ├── Makefile
        │           └── src
        ├── config
        │   ├── diode.json
        │   └── pv_list
        └── output

Each test scenario can be broken down into:

- Source IOC configuration.
- Epics-diode configuration.
- Receiving IOC configuration.
- Monitoring and analysis configuration.

Source IOC Configuration
^^^^^^^^^^^^^^^^^^^^^^^^
The test scenario POZ-side IOC (in ``$TESTROOT/volumes/bin/sourceIoc``) is used in the test run. Its database
should be configured to include ``poz:trigger`` record, that has initial DISA field set to 1 (disabled).
This record is used for starting and stopping the test run. The relevant value records should be named
``poz:v$(N)``, where ``$(N)`` is the number count.

The IOC should otherwise be configured as any other IOC (Do not forget to edit
``$TESTROOT/volumes/bin/sourceIoc/iocBoot/iocsource/st.cmd``.).

Epics-diode Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^
The ``$TESTROOT/volumes/config`` folder holds the particular test configuration. Edit ``diode.json`` according to
`Configuration <design.rst#configuration>`_.

Receiving IOC Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^
The receiving IOC config files reside in ``$TESTROOT/volumes/bin/destIoc``. No manual configuration is needed.
The ``xpoz.cmd`` file does not need to be changed. It uses ``test.db`` receiving IOC database file that is
automatically generated if absent from ``destIoc`` folder. It is recommended to delete the ``test.db`` file 
initially and let the script system generate it automatically (using ``diode_dbgen``) during ``up`` command to
``run_test.sh`` script. Only if the ``diode_dbgen`` was somehow unable to create satisfactory ``test.db``, it is
recommended to edit the file to make the receiving IOC work.


Monitoring and Analysis Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Monitoring
""""""""""

The monitoring part of the test scenario can be configured in two ways: `automated` or `manual`.

In `automated` mode the ``run_test.sh`` script executes ``start_dst_monitor.sh`` script which sets up ``camonitor``
command to monitor the receiving IOC PVs and outputs results in ``$TESTROOT/volumes/output/monitor.out``.

In `manual` mode, the ``run_test.sh`` script executes ``start_dst_monitor_manual.sh`` script which in turn executes
a custom script ``screen_monitor_command.sh`` located in ``$TESTROOT/volumes/bin/destIoc``. Here is an example of
such custom script:

.. code-block:: shell

    #!/bin/sh
    
    export EPICS_CA_AUTO_ADDR_LIST="no"
    export EPICS_CA_ADDR_LIST="xpoz"
    export PV_LIST=`cat /test_config/pv_list`
    while [ 1 ];
    do
      caget $PV_LIST
      sleep 1
    done

`Manual` mode can be used to set up any kind of custom monitoring. In the example above, the one second interval
``caget`` method is used. A `manual` mode is recognised as such by the script system by adding a dummy file
``manual_monitor`` in ``$TESTROOT`` folder.

Both methods use ``$TESTROOT/volumes/config/pv_list`` file to extract the arguments for ``camonitor`` or ``caget``
commands. Example ``pv_list`` file:

.. code-block:: shell

    -# 5 poz:v1 poz:v2 poz:v3 poz:v4 poz:v5  

Analysis
""""""""

The monitoring output is stored in ``monitor.out`` file in ``$TESTROOT/volumes/output/monitor.out``. During the
`report` phase of the ``run_test.sh`` script execution, the local (to the running test) script ``report.sh`` is
executed which normally runs ``analyze.py`` Python script. Both of these files are located in
``$TESTROOT``. The ``analyze.py`` accepts ``monitor.out`` as input, processes the output as per specific test
result requirements and produces an exit code as a result (0 - success, 1 - failure). An example ``analyze.py``
can be found in ``.template`` or any other test folder.




