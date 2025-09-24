Example
=======

.. code-block:: shell
    
    # start all-in-one demo IOC with POZ and XPOZ records, sender and receiver
    $ cd epics-diode/test/testDiodeApp/src
    $ ../../../bin/linux-x86_64/testDiode st.cmd 

    # a set of poz:<record name> and xpoz:<record name> records are created
    # use ca_monitor on xpoz records, and ca_put on poz records
    $ camonitor poz:ai1 xpoz:ai1
    poz:ai1                        2023-02-26 07:46:18.912078 9 HIHI MAJOR
    xpoz:ai1                       2023-02-26 07:46:18.912078 9 HIHI MAJOR
    poz:ai1                        2023-02-26 07:46:19.912044 0 LOLO MAJOR
    xpoz:ai1                       2023-02-26 07:46:19.912044 0 LOLO MAJOR
