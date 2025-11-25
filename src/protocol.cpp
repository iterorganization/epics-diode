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

#include <epics-diode/protocol.h>

namespace epics_diode {

Serializer& operator<<(Serializer& buf, const Header& h) {
    if (buf.ensure(Header::size)) {
        buf << h.magic;
        buf << h.global_seq_no;
        buf << h.startup_time;
        buf << h.config_hash;
    }
    return buf;
}

Serializer& operator>>(Serializer& buf, Header& h) {
    if (buf.ensure(Header::size)) {
        buf >> h.magic;
        buf >> h.global_seq_no;
        buf >> h.startup_time;
        buf >> h.config_hash;
    }
    return buf;
}

Serializer& operator<<(Serializer& buf, const SubmessageHeader& h) {
    if (buf.ensure(SubmessageHeader::size)) {
        buf << h.id;
        buf << h.flags;
        buf << h.bytes_to_next_header;
    }
    return buf;
}

Serializer& operator>>(Serializer& buf, SubmessageHeader& h) {
    if (buf.ensure(SubmessageHeader::size)) {
        buf >> h.id;
        buf >> h.flags;
        buf >> h.bytes_to_next_header;
    }
    return buf;
}

Serializer& operator<<(Serializer& buf, const CADataMessage& m) {
    if (buf.ensure(CADataMessage::size)) {
        buf << m.seq_no;
        buf << m.channel_count;
    }
    return buf;
}

Serializer& operator>>(Serializer& buf, CADataMessage& m) {
    if (buf.ensure(CADataMessage::size)) {
        buf >> m.seq_no;
        buf >> m.channel_count;
    }
    return buf;
}

Serializer& operator<<(Serializer& buf, const CAChannelData& m) {
    if (buf.ensure(CADataMessage::size)) {
        buf << m.id;
        buf << m.count;
        buf << m.type;
    }
    return buf;
}

Serializer& operator>>(Serializer& buf, CAChannelData& m) {
    if (buf.ensure(CADataMessage::size)) {
        buf >> m.id;
        buf >> m.count;
        buf >> m.type;
    }
    return buf;
}

Serializer& operator<<(Serializer& buf, const CAFragDataMessage& m) {
    if (buf.ensure(CAFragDataMessage::size)) {
        buf << m.seq_no;
        buf << m.fragment_seq_no;
        buf << m.channel_id;
        buf << m.count;
        buf << m.type;
        buf << m.fragment_size;
    }
    return buf;
}

Serializer& operator>>(Serializer& buf, CAFragDataMessage& m) {
    if (buf.ensure(CAFragDataMessage::size)) {
        buf >> m.seq_no;
        buf >> m.fragment_seq_no;
        buf >> m.channel_id;
        buf >> m.count;
        buf >> m.type;
        buf >> m.fragment_size;
    }
    return buf;
}


}



namespace {

size_t ilog2(std::size_t val)
{
    std::size_t ret = 0;
    while (val >>= 1) {
        ret++;
    }
    return ret;
}

size_t bits2bytes(std::size_t val)
{
    // round up to next multiple of 8
    val -= 1U;
    val |= 7U;
    val += 1U;
    // bits -> bytes
    val /= 8U;
    return val;
}

}

std::ostream& operator<<(std::ostream& strm, const epics_diode::Serializer& s)
{
    constexpr std::size_t groupBy = 4U;
    constexpr std::size_t perLine = 16U;

    const std::size_t len = s.remaining();
    // find address width in hex chars
    // find bit width, rounded up to 8 bits, divide down to bytes
    const std::size_t addrwidth = bits2bytes(ilog2(len)) * 2U;
    std::size_t nlines = len / perLine;

    if (len % perLine) {
        nlines++;
    }

    std::ios::fmtflags initialflags = strm.flags();
    strm << std::hex << std::setfill('0');
    for (std::size_t l = 0; l < nlines; l++)
    {
        std::size_t start = l * perLine;
        strm << "0x" << std::setw(addrwidth) << start;

        // print hex chars
        for (std::size_t col = 0; col < perLine; col++)
        {
            if(col % groupBy == 0) {
                strm << ' ';
            }
            
            if(start + col < len) {
                strm << std::setw(2) << unsigned(s[start + col] & 0xff);
            } else {
                strm << "  ";
            }
        }

        strm << ' ';

        // printable ascii
        for (std::size_t col= 0;  col < perLine && (start + col) < len; col++)
        {
            if (col % groupBy == 0) {
                strm << ' ';
            }
            char val = s[start + col] & 0xff;
            if (val >= ' ' && val <= '~') {
                strm << val;
            } else {
                strm << '.';
            }
        }

        strm << '\n';
    }
    strm.flags(initialflags);

    return strm;
}

void dump(std::ostream& strm, const epics_diode::Serializer& s) {
    strm << s;
}
