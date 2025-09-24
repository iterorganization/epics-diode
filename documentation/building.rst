Building
========

Dependencies
------------

- `EPICS Base <https://epics-controls.org/resources-and-support/base/>`_  >= 7.0.8
- `EPICS pvxs <https://github.com/epics-base/pvxs>`_  >= 1.3.1
- C++11 compliant compiler

Additionally, for testing:

- Docker (in rootless mode with docker-compose plugin)
- CODAC Core 3 Full Development docker image, version 7.2.0, loaded in local repository (named ``co3-full-devel:7.2.0``)

Build From Source
-----------------

Make sure that EPICS base and pvxs is already built (version >= 7.0.8 or newer) in your environment.

Obtain the source:

.. code-block:: shell

    # not yet available on epics-base GitHub repository
    $ git clone https://github.com/epics-base/epics-diode.git

Create ``RELEASE.local`` in the same directory as ``epics-diode`` or in ``epics-diode/configure`` containing reference to ``EPICS_BASE`` and ``PVXS``, e.g.:

.. code-block:: shell

    cat <<EOF > epics-diode/configure/RELEASE.local
    EPICS_BASE=\$(TOP)/../epics-base
    PVXS=\$(TOP)/../pvxs
    EOF

Build `epics-diode`:

.. code-block:: shell

    $ cd epics-diode
    $ make
