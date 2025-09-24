/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#ifndef EPICS_DIODE_PVA_PROTOCOL_H
#define EPICS_DIODE_PVA_PROTOCOL_H

#include <cstdint>
#include <vector>
#include <pvxs/data.h>

#include <epics-diode/protocol.h>

#include "bitmask.h"

namespace epics_diode {
namespace pva {
    
// TODO revise
// TODO add cache hash_key to general hash
typedef std::vector<::pvxs::Value> TypeCache;
void buildTypeCache(TypeCache& cache);  
uint16_t cacheType(TypeCache& cache, ::pvxs::Value &value);

struct TypeCache_ {
    static int16_t buildinCacheSize;
};

struct SubmessageType {
    enum ids : uint8_t {
        PVA_TYPEDEF_MESSAGE = 32,
        PVA_DATA_MESSAGE = 33
    };
};

// NOTE: must fit the buffer, no fragmentation supported
struct PVATypeDefMessage {
    static constexpr std::size_t size = 4;

    uint16_t start_id = 0;
    uint16_t typedef_count = 0;

    constexpr PVATypeDefMessage() {}

    constexpr explicit PVATypeDefMessage(uint32_t start_id, uint16_t typedef_count) : 
        start_id(start_id),
        typedef_count(typedef_count)
    {}
};

Serializer& operator<<(Serializer& buf, const PVATypeDefMessage& m);
Serializer& operator>>(Serializer& buf, PVATypeDefMessage& m);


struct TypeDefSerializer {
    static bool serialize(Serializer& buf, const ::pvxs::Value& type_value);
    static bool deserialize(Serializer& buf, ::pvxs::Value& type_value);
};




struct PVADataMessage {
    static constexpr std::size_t size = 4;

    uint16_t seq_no = 0;
    uint16_t channel_count = 0;  

    constexpr PVADataMessage() {}
    
    constexpr explicit PVADataMessage(uint16_t seq_no, uint16_t channel_count) : 
        seq_no(seq_no),
        channel_count(channel_count)
    {}
};

Serializer& operator<<(Serializer& buf, const PVADataMessage& m);
Serializer& operator>>(Serializer& buf, PVADataMessage& m);


enum UpdateType : uint8_t  {
    None = 0,
    Disconnected = 0,
    Partial = 1,
    Full = 2
};

struct PVAChannelData {
    static constexpr std::size_t size = 5;

    static constexpr std::size_t max_bitset_and_data_size = 
        MAX_MESSAGE_SIZE - 
        Header::size - SubmessageHeader::size - 
        PVADataMessage::size - PVAChannelData::size;

    uint32_t id = 0;
    uint16_t update_seq_no = 0; 
    UpdateType update_type = None;
    uint16_t type_id = 0; 

    constexpr PVAChannelData() {}
    
    constexpr explicit PVAChannelData(uint32_t id, uint16_t update_seq_no, UpdateType update_type, uint16_t type_id) : 
        id(id),
        update_seq_no(update_seq_no),
        update_type(update_type),
        type_id(type_id)
    {}

    bool serialize(Serializer& buf, const ::pvxs::Value& value, ::pvxs::BitMask* bitMask = nullptr);

};

Serializer& operator<<(Serializer& buf, const PVAChannelData& m);
Serializer& operator>>(Serializer& buf, PVAChannelData& m);

Serializer& operator>>(Serializer& buf, ::pvxs::Value& value);

}
}

#endif
