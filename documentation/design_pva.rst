Overview
========

Motivation
----------

EPICS pvAccess (PVA) diode is an extension of EPICS CA diode project to support
propagation of EPICS pvAccess traffic to remote (external, untrusted) sites. 
This document provides an overview of the design and specifies a unidirectional protocol
for the EPICS pvAccess diode.

Basic architecture
------------------

The PVA diode consists of three components: a *sender* that gathers PVA traffic from an internal network and sends it
using *unidirectional transport* to one-or-many *receivers* residing in an external network:

.. figure:: _static/images/pvadiode.drawio.svg
   :name: basic-arch
   :align: center
   :alt: PVA diode basic architecture image.

   PVA diode basic architecture.

Each component is described in detail in the following sections.

Design
======

Transport
---------

A key feature of `pvadiode` is unidirectional transport. This also brings disadvantages.
The limitations of unidirectional transports are:

- Unidirectional transports are inherently unreliable since they have no way of informing a sender whether a message was received or not.
  The sender gets no feedback from receivers. Loss of messages results in large latencies (waiting and hoping for the next message not to be lost).
  In the case of large array data, e.g. images, data needs to be fragmented to be sent over the network and a loss of just one fragment
  already implies a loss of the entire data. With a larger number of fragments probability of this happening increases. 
- Lack of feedback also implies there is no flow/congestion control. These mechanisms ensure that the sender will not overflow
  the receiver with messages (resulting in loss of messages) and allow (fair) coexistence of different senders on the network.
  Sudden bursts of traffic can also choke otherwise healthy and sufficient network links resulting in a degradation of the entire network.
  Since unidirectional protocols cannot implement these mechanisms they usually use a simple rate-limiting flow control where the maximum
  sending rate is limited, e.g. 10MB/s. This imposes a limit on transport throughput and increased latencies. 

Choosing unidirectional UDP as transport has (therefore) the following implications:

- a sender must resend the latest channel value every ``heartbeat period`` of time,
- not having received any update from one channel in 2x ``heartbeat period`` turns the channel value invalid (on the receiver side), 
- a mechanism to detect out-of-order, duplicate, or failed message delivery needs to be implemented (e.g. using message sequence number), 
- due to the limitation of the maximum UDP packet size messages exceeding this limit need to be fragmented (e.g. for large array values),
- rate-limited flow control needs to be implemented,
- consequently keep the amount of data sent to a minimum, i.e. avoid sending channel names and minimize sending channel metadata 
  over the network and cache them using static configuration or cache instead; both sides are required to use the same configuration.

Refer to the :doc:`protocol` document for protocol details.

PVA network protocol supports both publish/subscribe and request/response communication mechanisms. Due to the unidirectional limitation,
the `pvadiode` diode only supports publish/subscribe. The sender only uses `monitor` operation. The receiver provides `monitor` and `get` operations,
while `put` operations are denied, and `RPC` operations are not supported.

PVA operates on structured data, where structure type definitions are not fixed. Each channel has its own custom structure definition (also known as metadata),
which remains static throughout its lifetime. The metadata must be known to be able to de-/serialize the values the structure holds.

PVA supports partial updates, meaning only the fields of a structure that have changed are updated, reducing the amount of data transmitted over the network.

The overhead of the metadata compared to the actual data size is often significant.
For example, for a standard scalar channel (such as the NTScalar type), the serialized metadata is approximately 500 bytes,
the full structure value is around 200 bytes, and typical partial updates for values/timestamps are about 40 bytes.
The `pvadiode` protocol is optimized to minimize this overhead, caching metadata and only sending it periodically with full value updates, primarily using partial updates.

Each metadata definition is assigned an ID and stored in cache. This ID is only sent during full value updates and is retained on the receiver side
for use when partial updates are received. To promote interoperability at the application level, the EPICS community has defined a set of
standard high-level data types, known as normative types (NT-types). Given this, a predefined built-in cache can be utilized for NT-types,
greatly reducing or even eliminating the metadata overhead.

Periodically, the sender cache (excluding the built-in cache) must be synchronized with the receiver cache by transmitting all the cached metadata along with their corresponding IDs.

Sender
------

`Sender` is a PVA client subscribing to a predefined list of channels and pushing changes
to the transport. The list of channels is read from the `configuration`_ file - array node named ``channel_names``.
Each channel is assigned an ``index`` corresponding to its position in the array (starting with 0). This index is used
by the sender and receivers to identify channels.

On startup the sender creates a monitor (on-change value subscription) for all the channels in the list.
The first monitor event triggers a full value update, which includes the metadata. Subsequently, only partial updates are transmitted,
except during periodic full updates sent at each ``heartbeat_period`` period.

The implementation monitors the connection status of the channel. On disconnection, it marks
the channel as disconnected and puts a disconnect event in the send queue. 

A single worker thread is responsible for managing all events and sending messages. Value update and connection notifications must pass their tasks to the worker
thread by submitting a work request to the work queue.
The messages are sent periodically (``min_update_period``), thus limiting the maximum update frequency of channels to ``1 / min_update_period``.
Only updates for the channels that have been put into the send queue are being sent.  The implementation tries to fit as many as possible
updates into one packet (preserving send queue order). Once one channel data does not fit into a message buffer anymore the message is sent
and a new one is started. If a channel data value does not fit (i.e. is too large for) the message buffer, the data needs to be fragmented
and a protocol message that supports fragmentation is used. 
Once a channel is serialized to the message buffer, it is removed from the send queue and marked as cleared (i.e. no pending update).

The thread also sends heartbeat full value and metadata cache updates to mitigate the possible loss of updates due to unreliable protocol.
For disconnected or never-connected channels no update is sent; a receiver will mark channels without updates as disconnected.

Sending messages over UDP is rate-limited by the ``rate_limit_mbs`` configuration parameter. The rate-limiting is implemented by adding a time delay
between two consecutive sends. A required time delay not to exceed the limit is calculated  (``last_sent_bytes / rate_limit_mbs``) and compared
to the time elapsed since the last send. If the elapsed time is smaller than the required a process is being put to sleep for the remaining difference.
Note that the required delays are quite small, e.g. for 64k bytes of data at 64MB/s limit the required delay equals 1us.

Statistics are also gathered and reported for diagnostics, i.e. send rate, number/percentage of channels connected/updates within a heartbeat period.

Receiver
--------

`Receiver` listens to the transport for messages. Upon arrival, the messages are first validated.
Validation includes:

- `EPICS Diode` protocol message identification check, 
- one-sender check, and
- checks for out-of-order or duplicate delivery of a message.

Refer to the :doc:`protocol` document for protocol details.

The `receiver` functions as a `PVA` server. When the first full value update and valid metadata is received, a process variable is dynamically instantiated within the server.
Partial updates are accepted thereafter. If a partial or full update is missed, the process variable is marked as invalid and all subsequent partial updates
are ignored until the next full value update is received. The invalid process variable is closed (triggering a disconnect event for external clients),
if no updates are received within ``heartbeat_period``. Once a full value update is received, the process variable is reopened (becoming available again to external clients).

On every ``heartbeat_period`` period all the valid process variables are checked whether they received an update within the last ``2 * heartbeat_period`` period.
If not, the process variable is closed.

Configuration
-------------
Both `sender`_ and `receiver`_ are configured using the same JSON configuration file (inline documented):

.. code-block:: json
   :caption: EPICS Diode Configuration File

    {
      // Minimum sender update period in seconds.
      "min_update_period": 0.1,
      // Heartbeat period in seconds.
      "heartbeat_period": 15.0,
      // Maximum sender sent rate in MB/s, 0 for no limit.
      "rate_limit_mbs": 64,
      // Array of channels to export (order matters!).
      "channel_names": {
        // Each channel can be individually configured, otherwise defaults are used.
        "poz:ai1": {}, 
        "poz:ai2": {}, 
        "poz:ai3": {},
        "poz:image": {},
        "poz:enum": {}    
      }
    }

A hash value is calculated out of all attribute values and compared to ensure that they both share the same
configuration.

The configuration format is the same as for `epics-diode`. The channel configuration options that
are `epics-diode` specific (i.e. ``extra_fields`` and ``polled_fields``) will be ignored. Currently,
there are no channel configuration option, but the design allows future additions.

Implementation details
----------------------
`pvadiode` is a part of `epics-diode` module. It provides ``pvadiode_sender`` and ``pvadiode_receiver`` executables. Their main implementations,
``pva::Sender`` and ``pva::Receiver`` classes, are all bundled in ``epics-pva-diode`` shared library (source code in ``src/``). The implementation
depends on the ``epics-diode`` shared library that provides UDP transport, configuration, and logging utilities.

All the protocol de-/serialization code is encapsulated within ``src/epics-diode/pva/protocol.h`` and ``src/pva_protocol.cpp`` files. The implementation
depends pvxs library de-/serialization code. As this code is not officially part of a public API (the API is visible, but no headers are provided), 
releavant pvxs header files are shamelessly included (-I option). This is temporary solutuon until the API becomes officially public.

The entire code base is structured as EPICS base extension.

The default `pvadiode` UDP port is 5081.
