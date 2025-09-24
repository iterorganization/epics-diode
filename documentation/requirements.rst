Requirements
============

The following requirements and limitations were followed when designing and implementing `EPICS Diode`:

- use of unidirectional protocol (UDP),
- up to 50k channels expected (per instance),
- with a maximum update rate of 10Hz,
- maximum propagation delay must not exceed 100ms,
- the protocol code should be encapsulated to allow eventual protocol replacements, a pluggable design is not required
- Linux-based OS supported (might work on other platforms),
- dependency on EPICS Base >= 7.0.8,
- x86-64 compatible CPU,
- C++11 compliant compiler (C is used only for EPICS record and device support on the receiver side),
- sender-side must comply with SWIL-1 standards.

epics-diode (CA) specific
--------------------------
- optimized for scalar channels, but array values must be also supported,
- channel metadata (e.g. alarm limits) are transferred using configuration and not over the network,
- only `EPICS CA` ``DBR_TIME_*`` events are supported.


pvadiode specific
--------------------------
- due to unidirectional protocol, only subscription/get mechanism can be supported,
- support any custom structure type definition,
- transfer only changed fields (partial structure updates),
- dependency on EPICS pvxs >= 1.3.1.
