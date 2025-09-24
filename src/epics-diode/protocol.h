/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#ifndef EPICS_DIODE_PROTOCOL_H
#define EPICS_DIODE_PROTOCOL_H

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>

namespace epics_diode {
    
// parasoft-begin-suppress HICPP-3_5_1-c HICPP-3_5_1-d
// Suppress adding/subtracting pointers from different arrays.
// Pointers start, limit and pos are using the same array.
struct Serializer {

    using value_type = uint8_t;

    explicit Serializer(value_type* ptr, std::size_t size) :
        start(ptr),
        pos(ptr),
        limit(ptr + size),
        good(true)
    {}

    template<typename Container>
    explicit Serializer(Container &buffer) :
        Serializer(buffer.data(), buffer.size())
    {}

    virtual ~Serializer() {}

    inline size_t remaining() const {
        return limit - pos;
    }

    inline bool ensure(std::size_t n) {
        good &= (n <= remaining());
        return good;
    }

    explicit operator bool() const {
        return good;
    }

    inline void operator+=(std::size_t n) {
        pos += n;
    }

    inline value_type* data() const {
        return start;
    }

    inline size_t distance() const {
        return pos - start;
    }

    inline value_type& operator[](size_t ix) const {
        return pos[ix];
    }

    inline void write(const value_type *ptr, std::size_t len) {
        // since method is inlined the compiler will be able optimize memcpy
        memcpy(pos, ptr, len);
        pos += len;
    }

    inline void read(value_type *ptr, std::size_t len) {
        // since method is inlined the compiler will be able optimize memcpy
        memcpy(ptr, pos, len);
        pos += len;
    }

    inline Serializer& operator<<(value_type val) {
        *pos++ = val;
        return *this;
    }

    inline Serializer& operator>>(value_type& val) {
        val = *pos++;
        return *this;
    }

    template<typename T, typename std::enable_if<std::is_arithmetic<T>::value && sizeof(T) >= 2, int>::type = 0>
    Serializer& operator<<(const T& val) {
#ifdef REQUIRE_ALIGNED_ACCESS
        union {
            T val;
            value_type array[sizeof(T)];
        } flatrep;
        flatrep.val = val;
        write(flatrep.array, sizeof(T));
#else
        *reinterpret_cast<T*>(pos) = val;
        pos += sizeof(T);
#endif       
        return *this;
    }

    template<typename T, typename std::enable_if<std::is_arithmetic<T>::value && sizeof(T) >= 2, int>::type = 0>
    Serializer& operator>>(T& val) {
#ifdef REQUIRE_ALIGNED_ACCESS
        union {
            T val;
            value_type array[sizeof(T)];
        } flatrep;
        read(flatrep.array, sizeof(T));
        val = flatrep.val;
#else
        val = *reinterpret_cast<const T*>(pos);
        pos += sizeof(T);        
#endif       
        return *this;
    }

    template<std::size_t N>
    Serializer& operator<<(const std::array<value_type, N>& array) {
        write(array.data(), N);
        return *this;
    }

    template<std::size_t N>
    Serializer& operator>>(std::array<value_type, N>& array) {
        read(array.data(), N);
        return *this;
    }

    inline void pad_align(std::size_t alignment, value_type padding) {
        std::size_t n = distance() % alignment;
        if (n > 0) {
            for (; n < alignment; n++)
                *pos++ = padding;
        }
    }

    inline void pos_align(std::size_t alignment, value_type padding) {
        std::size_t n = distance() % alignment;
        if (n > 0) {
            pos += alignment - n;
        }
    }

    inline value_type* position() const {
        return pos;
    }

    inline void position(value_type* new_pos) {
        assert(new_pos >= start && new_pos <= limit);
        pos = new_pos;
    }

    inline bool try_position(value_type* new_pos) {
        if (new_pos >= start && new_pos <= limit) {
            pos = new_pos;
            return true;
        } else {
            return false;
        }
    }

protected:
    value_type *start;
    value_type *pos;
    value_type *limit;
    bool good;
};

// parasoft-end-suppress HICPP-3_5_1-c HICPP-3_5_1-d


struct Header {
    static constexpr std::size_t size = 24;
    
//    static constexpr std::array<uint8_t, 4> MAGIC = { 0x70u, 0x76u, 0x41u, 0x43u }; // 'pvAC' == pv 'anode-cathode' aka diode
    static constexpr uint8_t VERSION = 1;

    std::array<uint8_t, 4> magic{};
    uint8_t version = 0;
    std::array<uint8_t, 3> reserved{};
    std::uint64_t startup_time = 0;   //  time in milliseconds since the UNIX epoch, little-endian
    std::uint64_t config_hash = 0;    // configuration hash, little-endian

    /// Constructs empty (zeroed) header.
    constexpr Header() {}
    
    /// Constructs valid (with magic and versions) header with given GUID and configuration hash
    constexpr explicit Header(std::uint64_t startup_time, std::uint64_t config_hash) : 
        magic({ 0x70u, 0x76u, 0x41u, 0x43u }),
        version(VERSION),
        startup_time(startup_time),
        config_hash(config_hash)
    {}

    bool validate() const {
        static constexpr std::array<uint8_t, 4> MAGIC = { 0x70u, 0x76u, 0x41u, 0x43u };
        return (magic == MAGIC);
    }
};

Serializer& operator<<(Serializer& buf, const Header& h);
Serializer& operator>>(Serializer& buf, Header& h);


struct SubmessageType {
    enum ids : uint8_t {
        CA_DATA_MESSAGE = 16,
        CA_FRAG_DATA_MESSAGE = 17
    };
};

struct SubmessageFlag {
    enum mask : uint8_t {
        LittleEndian = 0x01
    };
};

// non-aligned: IPv4 = 65507, IPv6 = 65527 
constexpr std::size_t MAX_MESSAGE_SIZE = 65504;  // max. "8-byte aligned" UDP packet size

// Must be 8-byte aligned, zero padded.
struct SubmessageHeader {
    static constexpr std::size_t size = 4;
    static constexpr std::size_t alignment = 8;

    uint8_t id = 0;
    uint8_t flags = SubmessageFlag::LittleEndian;
    uint16_t bytes_to_next_header = 0;  // 0 means until the end of the message.

    constexpr SubmessageHeader() {}
    
    /*constexpr*/ explicit SubmessageHeader(uint8_t id, uint8_t flags, uint16_t bytes_to_next_header) : 
        id(id),
        flags(flags),
        bytes_to_next_header(bytes_to_next_header)
    {
        assert(bytes_to_next_header <= (MAX_MESSAGE_SIZE - Header::size - SubmessageHeader::size));
        assert(flags & SubmessageFlag::LittleEndian); // No support for big-endian for now (if ever).
    }
};

Serializer& operator<<(Serializer& buf, const SubmessageHeader& h);
Serializer& operator>>(Serializer& buf, SubmessageHeader& h);





struct CADataMessage {
    static constexpr std::size_t size = 4;

    uint16_t seq_no = 0;  // to detect out-of-order/duplicate delivery
    uint16_t channel_count = 0;  

    constexpr CADataMessage() {}
    
    constexpr explicit CADataMessage(uint16_t seq_no, uint16_t channel_count) : 
        seq_no(seq_no),
        channel_count(channel_count)
    {}
};

Serializer& operator<<(Serializer& buf, const CADataMessage& m);
Serializer& operator>>(Serializer& buf, CADataMessage& m);



struct CAChannelData {
    static constexpr std::size_t size = 8;

    static constexpr std::size_t max_data_size = 
        MAX_MESSAGE_SIZE - 
        Header::size - SubmessageHeader::size - 
        CADataMessage::size - CAChannelData::size;

    uint32_t id = 0;
    uint16_t count = 0;     // limited by max. size of UDP packet (and submessage size)
    uint16_t type = 0;
    //dbr_time_<type> data; // must always be 8-byte aligned
    //<padding>

    constexpr CAChannelData() {}
    
    constexpr explicit CAChannelData(uint32_t id, uint16_t count, uint16_t type) : 
        id(id),
        count(count),
        type(type)
    {}
};

Serializer& operator<<(Serializer& buf, const CAChannelData& m);
Serializer& operator>>(Serializer& buf, CAChannelData& m);



struct CAFragDataMessage {
    static constexpr std::size_t size = 16;

    uint16_t seq_no = 0;    // must be same for all fragments
    uint16_t fragment_seq_no = 0;  
    uint32_t channel_id = 0;
    uint32_t count = 0;
    uint16_t type = 0;
    uint16_t fragment_size = 0;

    constexpr CAFragDataMessage() {}
    
    constexpr explicit CAFragDataMessage(
        uint16_t seq_no, uint16_t fragment_seq_no,
        uint32_t channel_id, uint32_t count, uint16_t type,
        uint16_t fragment_size) : 
        seq_no(seq_no),
        fragment_seq_no(fragment_seq_no),
        channel_id(channel_id),
        count(count),
        type(type),
        fragment_size(fragment_size)
    {}
};

Serializer& operator<<(Serializer& buf, const CAFragDataMessage& m);
Serializer& operator>>(Serializer& buf, CAFragDataMessage& m);

}

std::ostream& operator<<(std::ostream& strm, const epics_diode::Serializer& s);
void dump(std::ostream& strm, const epics_diode::Serializer& s);

#endif
