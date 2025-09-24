EPICS Diode Protocol
====================

This document describes the `EPICS Diode` protocol. The protocol is designed to run over a unidirectional
and unreliable transport such as UDP/IP, and used to send messages (transported as datagrams in packets)
from sender to one-or-many receivers.

The main purpose of the protocol is to implement a unidirectional transfer of `EPICS channel` value changes,
usually transported using a bidirectional TCP/IP-based protocol `EPICS Channel Access (CA)` and `EPICS pvAccess (PVA)`, between segmented networks.
This allows placing a unidirectional network communication device (aka diode) that enables the safe, one-way transfer of data between segmented networks.
The diodes effectively eliminate external points of entry to the sending system, preventing intruders and contagious elements from infiltrating the network.

The protocol structure is inspired by `Real-time Publish-Subscribe Protocol (RTPS) <https://www.omg.org/spec/DDSI-RTPS>`_ but simplified and adapted to unidirectional transport.
This structure allows the protocol to be extended (adding new submessages) without affecting older revisions,
i.e. adding support for `pvAccess` will not break existing (older) receivers.

Each message, i.e. UDP packet, consists of a ``Header`` followed by one-or-many ``Submessage``-s:

.. code-block:: c++

    struct Message {
        Header header;
        Submessage submessages[];
    }

Further on, each ``Submessage`` consists of ``SubmessageHeader`` and its payload.

.. code-block:: c++

    struct Submessage {
        SubmessageHeader subheader;
        byte payload[];
    }

Header
------

The ``Header`` is defined as follows:

.. code-block:: c++

    struct Header {
        uint8_t magic[4] = { 0x70u, 0x76u, 0x41u, 0x43u }; // 'pvAC' == pv 'anode-cathode' aka diode
        uint8_t version = 1;          // current revision number
        uint8_t reserved[3];          // not used
        std::uint64_t startup_time;   // time in milliseconds since the UNIX epoch, little-endian
        std::uint64_t config_hash;    // configuration hash, little-endian, 0 means check is disabled
    }

A message must start with a predefined 4-byte ``magic`` sequence identifying `EPICS Diode` protocol.
If a message does not start with this sequence it is not a valid message and must be dropped.

The ``version`` identifies the revision of the protocol used in the message. All revisions must be backward compatible;
if they are not then different ``magic`` must be used.

The ``startup_time`` field holds the time when a sender was started.
It is defined as the time in `milliseconds since the UNIX epoch (January 1, 1970 00:00:00 UTC) <https://currentmillis.com/>`_ and
must be little-endian encoded (as most modern CPUs are little-endian).
It serves to differentiate senders from each other (as a quasi-identifier) with 
the added ability to identify the most recent sender. One receiver should accept messages only from
one sender - the sender with the latest ``startup_time``. Messages from other senders must be dropped.
This covers common use-cases where a sender process is restarted or started multiple times.

If multiple "streams" are required then multiple (sender, receiver) instances can be instantiated
within one process (each operating on a separate port). This provides required isolation among them,
i.e. avoid sharing the same UDP send/receive buffers within OS network stack.

The ``config_hash`` field holds a hash value of used configuration. The value must be little-endian encoded.
Hash values of a sender and receiver can be compared to check whether the same configuration is being used.
If this check is not needed a hash of value 0 can be used to disable it.

Note that the ``Header`` is static for the entire lifecycle of a sender.


Submessage Header
-----------------

The ``Submessage`` is defined as follows:
 
.. code-block:: c++

    struct SubmessageHeader {
        uint8_t id;
        uint8_t flags;                  // LSB indicates endianess { 0 - big, 1 - little }
        uint16_t bytes_to_next_header;  // 0 means until the end of the message.
    }

The ``id`` field determines the actual type of a submessage, each being described in the following sections.
A set of flags is stored within ``flags`` field, currently only LSB bit is being used. The LSB bit indicates
what encoding is being used within this submessage, 0 for big- and 1 for little-endian encoding.

A submessage is followed by a payload. The size of a payload is specified in ``bytes_to_next_header`` field.
A value of 0 implies "until the end of the message". The protocol requires that all ``Submessage``-s are 8-byte aligned,
i.e. start of each ``Submessage`` must be at memory offset that is multiple of 8 compared to the start of the message.
If the actual payload size does not end just before 8-byte boundary it must be padded. The alignment requirement allows
optimized de-/serialization from/to the message.

If a receiver detects an unknown ``Submessage`` type, the ``Submessage`` must be simply ignored by
advancing for `bytes_to_next_header` bytes (skipping over the payload). This provides interoperability among
different protocol versions.

Note that `bytes_to_next_header` is a 16-bit unsigned integer. This limits the maximum ``Submessage`` payload size to::

    65536 - sizeof(Header) - sizeof(SubmessageHeader) = 65536 - 24 - 4 = 65508 bytes.

UDP maximum payload size is 65507 bytes for IPv4 and 65527 bytes for IPv6,
therefore maximum (IPv4) 8-byte aligned ``Submessage`` payload size is limited to 65504 bytes.

Submessages
-----------

Version 1 prescribes the following ``Submessage`` types:

.. code-block:: c++

    struct SubmessageType {
        enum ids : uint8_t {
            // CA
            CA_DATA_MESSAGE = 16,
            CA_FRAG_DATA_MESSAGE = 17,
            // PVA
            PVA_TYPEDEF_MESSAGE = 32,
            PVA_DATA_MESSAGE = 33,
            PVA_FRAG_DATA_MESSAGE = 34
        };
    };

Values from 0 - 15 are reserved for future "internal" usage.

CADataMessage (16)
~~~~~~~~~~~~~~~~~~

This ``Submessage`` carries a set of `EPICS CA` channel updates. 

.. code-block:: c++

    struct CADataMessage {
        uint16_t seq_no;
        uint16_t channel_count; 
        CAChannelData channel_updates[channel_count]; 
    }

The ``seq_no`` field is an incrementing counter increased for each ``CADataMessage`` or ``CAFragDataMessage`` for the entire set of fragments (described in the following section).
This allows the detection of out-of-order or duplicate deliveries (which can happen when using UDP). Such submessages must be ignored. 
On overflow ``seq_no`` must restart with 0. This must be properly handled by the implementation, and not misinterpreted as out-of-order delivery.

The ``channel_updates`` field specifies the number of ``CAChannelData`` structures to follow (each for each channel update) and the structure is defined as:

.. code-block:: c++

    struct CAChannelData {
        uint32_t channel_id;
        uint16_t count;
        uint16_t type;
        dbr_type data;    // CA DBR type value representation, always 8-byte aligned and padded
    }

The ``channel_id`` field identifies a channel whose update this is. Both sender and receiver must agree on the mapping of channel ``channel_id``-s.
This can be simply done via sharing the same configuration that holds that mapping, however this is out-of-scope from the protocol perspective.
The field ``count`` specifies the number of elements and ``type`` CA DBR type of the following ``data`` field.

``data`` field is CA DBR type memory representation of CA DBR value structure, i.e. a ``memcpy`` can be used to de-/serialize data from/to the message.
In addition ``Submessage`` alignment 8-byte requirement, ``CADataMessage`` and ``CAChannelData`` carefully chosen structure sizes make sure that
``data`` field is always 8-byte aligned and padded (as designed by the CA protocol). This makes a highly efficient serialization of CA values.

CAFragDataMessage (17)
~~~~~~~~~~~~~~~~~~~~~~

When CA value update ``data`` is larger and does not fit into the maximum ``Submessage`` payload size (or remaining of a message buffer)
the data needs to be fragmented. This submessage supports that (for only one channel update), whereas ``CADataMessage`` does not.

.. code-block:: c++

    struct CAFragDataMessage {
        uint16_t seq_no;
        uint16_t fragment_seq_no;  
        uint32_t channel_id;
        uint32_t count;
        uint16_t type;
        uint16_t fragment_size;    
        uint8_t fragment[fragment_size];    // always 8-byte aligned
    }

The ``seq_no``, ``channel_id``, ``count``, and ``type`` fields follow the same rules as described for ``CADataMessage`` submessage.
All these fields must be the same for all the fragments. It is the ``fragment_seq_no`` field that specifies a sequence number of 
a fragment (starting with 0, limited to a total of 65536 fragments). Fragments must be received in order and without any
missing fragments, if not then all the ``CAFragDataMessage`` submessages with the same ``seq_no`` must be ignored.

The ``fragment-size`` field specifies the number of bytes in each fragment (can be different for each fragment).
A total data size can be calculated using ``count`` and ``type`` fields. When a sum of all ``fragment_size``-s reaches
the calculated total data size all fragments are considered to be received.

PVATypeDefMessage (32)
~~~~~~~~~~~~~~~~~~~~~~~

This ``Submessage`` carries a set of `PVA` structure typedef description (aka metadata) definitions. 

.. code-block:: c++

    struct PVATypeDefMessage {
        uint16_t start_id;
        uint16_t typedef_count;
        <structure type defintion>[typedef_count];    // serialized structure type definition
    }

The message allows definition of multiple structure types. The ``typedef_count`` field specifies the number of types to be defined. 
Each type gets assigned an ID: the first type ID is ``start_id``, next ``start_id + 1``,... , the last ``start_id + typedef_count - 1``.
The structure type definition is serialized according the ``PVA`` type definition serialization.

Note that the message does not support fragmentation. This implies that the structures with extreemly large type definitions,
i.e. larger that maximum UDP packet size, are not supported.

PVADataMessage (33)
~~~~~~~~~~~~~~~~~~~~

This ``Submessage`` carries a set of `PVA` channel updates. 

.. code-block:: c++

    struct PVADataMessage {
        uint16_t seq_no;
        uint16_t channel_count; 
        PVAChannelData channel_updates[channel_count]; 
    }

The ``seq_no`` field is an incrementing counter increased for each ``PVADataMessage`` or ``PVAFragDataMessage`` for the entire set of fragments (described in the following section).
This allows the detection of out-of-order or duplicate deliveries (which can happen when using UDP). Such submessages must be ignored. 
On overflow ``seq_no`` must restart with 0. This must be properly handled by the implementation, and not misinterpreted as out-of-order delivery.

The ``channel_updates`` field specifies the number of ``PVAChannelData`` structures to follow (each for each channel update) and the structure is defined as:

.. code-block:: c++

    enum UpdateType : uint8_t  {
        Disconnected = 0,
        Partial = 1,
        Full = 2
    };

    struct PVAChannelData {
        uint32_t channel_id;
        uint16_t update_seq_no; 
        UpdateType update_type;        

        case (update_type) {
          switch Full: 
            uint16_t type_id;
            // NOTE: Full includes Partial update fields, hence no 'break' here
          switch Partial: 
            ::pvxs::BitMask bit_mask;
            ::pvxs::Value value;
            break;
          switch Disconnected:
            break;
        }
    }

The ``channel_id`` field identifies a channel whose update this is. Both sender and receiver must agree on the mapping of channel ``channel_id``-s.
This can be simply done via sharing the same configuration that holds that mapping, however this is out-of-scope from the protocol perspective.

The ``update_type`` determines the type of the update: ``Disconnected`` (disconnect notification), ``Partial`` (partial value update), and
``Full`` (full value update including structure type definition ID). If ``type_id`` is not known yet (defined using ``PVATypeDefMessage`` message),
the update must be ignored.

The ``update_seq_no`` field is an incrementing counter increased for each ``PVAChannelData`` for particual ``channel_id``. This allows
the detection of missed partial updates. When a partial update miss is detected, all the subsequent partial updates must be ignored
until a full value update is received. On overflow ``update_seq_no`` must restart with 0.

The ``type_id`` is an identification of a structure type definition (metadata) defined using  ``PVATypeDefMessage`` message. 
The ``bit_mask`` and ``value`` fields carry an actual value update, serialized according the ``PVA`` data serialization.


PVAFragDataMessage (34)
~~~~~~~~~~~~~~~~~~~~~~~~

When `PVA` value update ``data`` is larger and does not fit into the maximum ``Submessage`` payload size (or remaining of a message buffer)
the data needs to be fragmented. This submessage supports that (for only one channel update), whereas ``PVADataMessage`` does not.

.. code-block:: c++

    enum FragmentFlags : uint8_t  {
        LastFragment = 0x01,
        Unused1 = 0x02,
        Unused2 = 0x04,
        Unused3 = 0x08,
        Unused4 = 0x10,
        Unused5 = 0x20,
        Unused6 = 0x40,
        Unused7 = 0x80
    };

    struct PVAFragDataMessage {
        uint16_t seq_no;
        uint16_t fragment_seq_no;  
        uint32_t channel_id;
        uint16_t update_seq_no; 
        UpdateType update_type;        
        FragmentFlags flags;
        uint16_t fragment_size;    
        uint8_t fragment[fragment_size];    // payload
    }

The ``seq_no``, ``channel_id``, ``update_seq_no``, and ``update_type`` fields follow the same rules as described for ``PVADataMessage`` submessage.
All these fields must be the same for all the fragments. It is the ``fragment_seq_no`` field that specifies a sequence number of 
a fragment (starting with 0, limited to a total of 65536 fragments). Fragments must be received in order and without any
missing fragments, if not then all the ``PVAFragDataMessage`` submessages with the same ``seq_no`` must be ignored.

The ``fragment-size`` field specifies the number of bytes in each fragment (can be different for each fragment).
The ``flags`` field is a bitmask-encoded field, currently used only to indicate the last fragment. The message is 
intentionally designed not to provide the total size of all fragments in advance, preventing the sender from determining it.