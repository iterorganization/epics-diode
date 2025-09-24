Quality Assurance / Testing Proposal
====================================

Functional Tests
----------------

Epics-diode is a server/client application, therefore not particularly suitable to unit testing.
Especially since there are almost no network decoupled functions, which are difficult and time
consuming to mock for unit testing. Code would have to be (at least partially) modified to allow
for private method testing. Since all the functionality can be tested with specially prepared
integration tests, and at the same time provides unified test environment/framework for all the tests
(changing only custom test startup parameters), we suggest integration tests to test the diode's
functionality and provide code coverage. The tests would be specially prepared to test all
threshold conditions. Also, the tests would have to be prepared for automated testing.


Unit Tests
^^^^^^^^^^

- Configuration: The test would include parsing configuratio file: diode.json. The test can be prepared in such a way, that it tests valid and invalid config files with various configurations.


Integration Tests
^^^^^^^^^^^^^^^^^

A common integration test environment/framework would have to be implemented. Docker would be a viable
solution, where POZ and XPOZ sides would each reside in their respective containers and Docker network
would be prepared to let them communicate through UDP.
Each test run would be executed by a start script, taking into account the current test's Dockerfile 
(with the appropriately mounted volumes), run the test, and 'export' the result to a pre-set volume.
The base docker images for POZ and XPOZ would be CODAC servers.
The tests would be split in two types. The first would concentrate on testing diode main functionality,
unidirectional packet transfer. For the first series of tests the Hollow IOC would not be needed and a setup
with sender and receiver (diode-receiver) would be used (POZ: Sender IOC, Diode sender; XPOZ: Diode
receiver.). Diode-receiver reads diode.json configuration,
listens to provided UDP port, decodes incoming traffic and displays (outputs) the receiving packets.
This output can later be used for analysis and comparison with the expected results.
The second type of integration tests setup would have to include the Hollow IOC. Thus the 'full' setup
would be used (POZ: Source IOC, Diode sender; XPOZ: Diode receiver (integrated with IOC), Diode IOC.).
In the second test setup, CA protocol would have to be used to read the Source IOC PVs and compare them
to the Diode IOC ones.
Some examples of integration tests (type one - with diode receiver on XPOZ side):

- Send one PV change, check the result.
- Send one array PV change, check the result.
- Send a few PV changes at once (but the total amount if data within one UDP packet).
- Send so many PV changes at once, that they exceed one UDP package size. This numbers can vary.
- Send many array PV changes at once.
- Subscribe to 'extra' fields and check all (most) of the above with 'extra' fields.
- Repeat similarly with 'polled' fields.
- Update source PVs' timestamps only and test that behaviour. 
- Update source PVs' alarm state and test that behaviour. 
- Test where only one field changes, but the whole channel must be send (tested for the main, extra and polled fields).
- Heartbeat test: The test would have to run for the heartbeat amount of time (at least) and check whether heartbeat works properly (sends everything).

For the second type of test (full Hollow IOC) connected, there would have to be the complete setup prepared (config file
and DB XPOZ creation). Eventually all of the above tests could be repeated, but this time checked via CA tools
(caget, camonitor).
Additional tests for the receiving (hollow) IOC could be prepared if deemed necessary. 



Performance Tests
-----------------
The existing performance tests have mostly given us the understanding that the operating system and hardware configuration
play the most important roles in actual/real performance. Therefore, performance testing would at the same time
represent performance tuning. During various setups and configurations, the most appropriate OS/network parameters
would be selected. Once the OS/network setup is complete, the already prepared tests regarding latencies and packet drop,
could be rerun.



Non-functional Tests
--------------------

All of the non-functional tests could be set up similarly to integration tests above (With Docker containers
and scripts.). It is just that the actual tests would be conducted differently and different resulting
outputs would be analyzed. 

Stress Tests
^^^^^^^^^^^^

The basic premise of a stress test is the quantity of data. Hence, we would prepare a test setup with a large number
of PVs and high update frequency and check how diode handles when under stress.

- A large number of simple PVs (no arrays), with various frequencies and sending times.
- A large number of array type PVs, with various frequencies and sending times.
- A mix of simple and array type PVs, again with various sending frequencies and sending times.


Memory Tests
^^^^^^^^^^^^

Once the aforementioned setup (Docker, scripts) is prepared, we could use it for all the other tests as well.
The main tool we suggest for memory analysis is 'valgrind'. It detects memory leaks and other potential memory
handling issues. The tool is simple to use, it is basically just prepended to the usual command line.
The resulting textual output can later be analyzed. Some of the tests suggested:

- Running 'diode-sender' with a few different configurations (simple, array, extra-fields, polled-fields) and a few different durations (10s, 1min, 5min).
- Running 'diode-receiver' (can be run at the same time as the 'diode-sender').
- Replacing 'diode-receiver' with the Hollow IOC and rerun the tests above.


Long Running Tests
^^^^^^^^^^^^^^^^^^

Considering the specifics for long running tests, we suggest those to be run manually. The test setup would
be provided (but it could always be changed by the operator). Logs would be stored and analyzed afterwards. 
It important to note, that long running tests are not always the best for 'valgrind' analysis. Perhaps
some other OS (or otherwise) provided tool could be used for port mortem memory analysis. The rest of the
logged data, could easily be parsed automatically and results presented.



Security
^^^^^^^^

At this time we do not find any specific need for security related tests.


