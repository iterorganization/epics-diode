/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */


#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <ostream>

#include <epics-diode/pva/protocol.h>

#include <pvxs/nt.h>

#include "bitmask.h"
#include "dataimpl.h"
#include "pvaproto.h"

// pvxs source re-use workaround
namespace pvxs {

BitMask::BitMask(BitMask&& o) noexcept
    :_words(std::move(o._words))
    ,_size(o._size)
{
    o._size = 0u;
}

}

namespace epics_diode {
namespace pva {

int16_t TypeCache_::buildinCacheSize = 0;

#if PVXS_VERSION >= VERSION_INT(1, 3, 0, 0)
#define FORMAT_PARAM , true
#else
#define FORMAT_PARAM
#endif

void buildTypeCache(TypeCache& cache)
{
    cache.reserve(128);
    // TODO all these are constexpr, use this
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::Bool, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::BoolA, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::Int8, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::Int16, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::Int32, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::Int64, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::UInt8, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::UInt16, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::UInt32, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::UInt64, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::Int8A, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::Int16A, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::Int32A, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::Int64A, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::UInt8A, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::UInt16A, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::UInt32A, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::UInt64A, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::Float32, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::Float64, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::Float32A, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::Float64A, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::String, true, true, true FORMAT_PARAM}.build().create());
    cache.emplace_back(::pvxs::nt::NTScalar{::pvxs::TypeCode::StringA, true, true, true FORMAT_PARAM}.build().create());

    cache.emplace_back(::pvxs::nt::NTEnum{}.build().create());
    cache.emplace_back(::pvxs::nt::NTNDArray{}.build().create());

    TypeCache_::buildinCacheSize = cache.size();
}

uint16_t cacheType(TypeCache& cache, ::pvxs::Value &value)
{
    uint16_t id = 0;
    for (auto& cached_value : cache) {
        if (value.equalType(cached_value)) {
            return id;
        }
        id++;
    }

    cache.emplace_back(value.cloneEmpty());
    return id;
}



Serializer& operator<<(Serializer& buf, const PVATypeDefMessage& m) {
    if (buf.ensure(PVATypeDefMessage::size)) {
        buf << m.start_id;
        buf << m.typedef_count;
    }
    return buf;
}

Serializer& operator>>(Serializer& buf, PVATypeDefMessage& m) {
    if (buf.ensure(PVATypeDefMessage::size)) {
        buf >> m.start_id;
        buf >> m.typedef_count;
    }
    return buf;
}


bool TypeDefSerializer::serialize(Serializer& buf, const ::pvxs::Value& type_value)
{
    // TODO ensure
    ::pvxs::impl::FixedBuf s(false, buf.position(), buf.remaining());
    auto type = ::pvxs::Value::Helper::desc(type_value);
    ::pvxs::to_wire(s, type);
    buf.position(s.save());
    return s.good();
}

bool TypeDefSerializer::deserialize(Serializer& buf, ::pvxs::Value& type_value)
{
    // TODO ensure
    ::pvxs::TypeStore cache;
    ::pvxs::impl::FixedBuf s(false, buf.position(), buf.remaining());
    ::pvxs::from_wire_type(s, cache, type_value);
    buf.position(s.save());
    return s.good();
}



Serializer& operator<<(Serializer& buf, const PVADataMessage& m) {
    if (buf.ensure(PVADataMessage::size)) {
        buf << m.seq_no;
        buf << m.channel_count;
    }
    return buf;
}

Serializer& operator>>(Serializer& buf, PVADataMessage& m) {
    if (buf.ensure(PVADataMessage::size)) {
        buf >> m.seq_no;
        buf >> m.channel_count;
    }
    return buf;
}

Serializer& operator<<(Serializer& buf, const PVAChannelData& m) {
    if (buf.ensure(PVAChannelData::size)) {
        buf << m.id;
        buf << m.update_seq_no;
        buf << uint8_t(m.update_type);

        if (m.update_type != Disconnected) {

            if (m.update_type == Full) {
                if (buf.ensure(sizeof(m.type_id))) {
                    buf << m.type_id;
                }
            }
        }
    }
    return buf;
}

bool PVAChannelData::serialize(Serializer& buf, const ::pvxs::Value& value, ::pvxs::BitMask* bitMask)
{
    if (buf && update_type != Disconnected) {
        ::pvxs::impl::FixedBuf s(false, buf.position(), buf.remaining());
        ::pvxs::to_wire_valid(s, value, bitMask);       // TODO no need to calculate valid again
        buf.position(s.save());
    }
    return buf.operator bool();
}


Serializer& operator>>(Serializer& buf, PVAChannelData& m) {
    if (buf.ensure(PVAChannelData::size)) {
        buf >> m.id;
        buf >> m.update_seq_no;
        uint8_t ut;
        buf >> ut;
        m.update_type = UpdateType(ut);

        if (m.update_type == Full) {
            if (buf.ensure(sizeof(m.type_id))) {
                buf >> m.type_id;
            }
        }
    }
    return buf;
}


Serializer& operator>>(Serializer& buf, ::pvxs::Value& value) {
    ::pvxs::impl::FixedBuf s(false, buf.position(), buf.remaining());

    // type deserialization
    ::pvxs::TypeStore cache;
    from_wire_valid(s, cache, value);

    buf.position(s.save());

    return buf;
}


}
}
