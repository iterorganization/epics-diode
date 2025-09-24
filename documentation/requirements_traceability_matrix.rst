Requirements Traceability Matrix
================================

.. tabularcolumns:: |p{2cm}|p{3cm}|p{5cm}|p{5cm}|p{5cm}|

.. csv-table:: "Requirements Traceability Matrix"
    :class: longtable
    :widths: 1 3 6 6 6
    :header: "Req. Number", "Req. Type", "Description", "Solution", "Comment"

    "R01",  "TECHNICAL",        "Unidirectional protocol must be used (UDP).", "UDP protocol is used.", "Check `Design`, section `Transport`, and in code: `src/transport.cpp`."
    "R02",  "PERFORMANCE",      "The diode must handle up  to 50k channels (per instance).", "Implemented.", "Check performance related comment [1]_."
    "R03",  "PERFORMANCE",      "Maximum update rate of 10Hz must be supported (on up to 50k channels).", "Implemented.", "Check performance related comment [1]_."
    "R04",  "PERFORMANCE",      "Maximum propagation delay  must not exceed 100ms,", "Implemented.", "Check performance related comment [1]_."
    "R05",  "FUNCTIONAL",       "Although optimized for scalar channels, array values must also be supported.", "Diode supports array data. When data size exceeds UDP packer size, the data is fragmented into many packets.", "Check `Design`, section `Transport`, and in code: `src/transport.cpp`, `src/sender.cpp`, and `src/receiver.cpp`."
    "R06",  "FUNCTIONAL",       "Channel metadata (e.g. alarm limits) should be transferred using  configuration and not through the network,", "Channel metadata is encoded in the receiver's database. It is never propagated through the network.", "Check `Design`, section `Transport`."
    "R07",  "TECHNICAL",        "The protocol code should be encapsulated to allow eventual protocol, replacements. (A pluggable design is not required.)", "Protocol resides in separate classes and could potentially be replaced relativelly easy.", "Check code: `src/protocol.cpp` and `src/epics-diode/protocol.h`."
    "R08",  "TECHNICAL",        "Sender should only use `EPICS Channel Access (CA)` for data acquisition.", "Confirmed. Sender is an EPICS client and only uses CA to access data.", "Check `Design`, section `Sender`."
    "R09",  "TECHNICAL",        "Only `EPICS CA` ``DBR_TIME_*`` events should be supported,", "Indeed, diode only supports those.", "Check `Design`, section `Sender`."
    "R10",  "TECHNICAL",        "Linux-based OS must be supported (other platforms are optional).", "Confirmed. All development and testing was Linux based.", ""
    "R11",  "TECHNICAL",        "It must be able to run on EPICS Base >= 7.0.7.", "Confirmed.", "Check `configure/RELEASE.local`."
    "R12",  "TECHNICAL",        "A x86-64 compatible CPU must be supported.", "Confirmed.", ""
    "R13",  "TECHNICAL",        "It must be able to compile on C++11 compliant compiler.", "Confirmed.", "Check `configure/CONFIG_SITE`."
    "R14",  "QUALITY ASSURANCE","Sender-side must comply with SWIL-1 standards.", "Using Parasoft analysis, we located and fixed `blocker` and `critical` bugs, and given the limited time resources, tried to reduce the number of major bugs.", "Code quality is an ongoing process. We will try to reduce the reported bug number on each commit."
    "Some additional requirements were adopted in final version."
    "R20",  "FUNCTIONAL",       "Support for additional (non-default) fields must be added.", "`extra_fields` configuration file parameter was added and 'non-default' field handling implemented.", "Bugzilla issue: `https://bugzilla.iter.org/codac/show_bug.cgi?id=15937`"
    "R21",  "FUNCTIONAL",       "Support for polled fields (those that cannot or will not be subscribed  to).", "`polled_fields` configuration file parameter was added and the polling process was integrated with the 'normal' subscriptions.", "Check code: `src/sender.cpp'."
    "R22",  "FUNCTIONAL",       "Receiver must implement a so-called hollowed-out IOC (record processing removed).", "A modified version of EPICS IOC was implemented, one with the record processing functions removed.", "Bugzilla issue: `https://bugzilla.iter.org/codac/show_bug.cgi?id=15936`"


.. [1] Special care must be taken when evaluating diode performance. The operating system and network configuration play significant role in the actual throughput and the number of
   dropped packets. Each network component (and the coresponding OS driver) with its internal packet sizes, buffers, caches etc. adds its layer of complextity and potential
   bottlenecks.
