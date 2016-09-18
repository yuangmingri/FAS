#ifndef __RTP_MEDIA_STREAM_PROCESSING_HPP__
#define __RTP_MEDIA_STREAM_PROCESSING_HPP__

#include <iostream>
#include <set>
#include "include.hpp"



void print_rtp_header_details(const struct network_connection&, struct rtp_header*, uint16_t);
extern std::set<Codec> allowed_codecs;


inline
uint16_t get_rtp_version(const struct rtp_header* hdr)
{
    std::bitset<2> bs; 
    bs[0] = hdr->control & (1 << 6);
    bs[1] = hdr->control & (1 << 7);

    return bs.to_ulong();
}

//----------------------------------------------------------------------

inline
bool get_rtp_padding_flag(struct rtp_header* hdr)
{
    if (hdr->control & (1 << 5)) {
        return true;
    }

    return false;
}
//----------------------------------------------------------------------

inline
bool get_rtp_extension_flag(struct rtp_header* hdr)
{
    if (hdr->control & (1 << 4)) {
        return true;
    }

    return false;
}

//----------------------------------------------------------------------

inline
uint16_t get_rtp_csrc_count(struct rtp_header* hdr)
{
    std::bitset<4> bs; 
    bs[0] = hdr->control & (1 << 0);
    bs[1] = hdr->control & (1 << 1);
    bs[2] = hdr->control & (1 << 2);
    bs[3] = hdr->control & (1 << 3);

    return bs.to_ulong();
}

//----------------------------------------------------------------------

inline
uint16_t get_rtp_market_flag(struct rtp_header* hdr)
{
    if (hdr->payload & (1 << 7)) {
        return true;
    }

    return false;
}

//----------------------------------------------------------------------

inline
uint16_t get_rtp_payload_type(const struct rtp_header* hdr)
{
    std::bitset<7> bs; 
    bs[0] = hdr->payload & (1 << 0);
    bs[1] = hdr->payload & (1 << 1);
    bs[2] = hdr->payload & (1 << 2);
    bs[3] = hdr->payload & (1 << 3);
    bs[4] = hdr->payload & (1 << 4);
    bs[5] = hdr->payload & (1 << 5);
    bs[6] = hdr->payload & (1 << 6);

    return bs.to_ulong();
}

//----------------------------------------------------------------------

inline
uint16_t get_rtp_header_len(const struct rtp_header*)
{
    return RTP_HEADER_MIN_LEN;
}

//----------------------------------------------------------------------

inline
bool is_rtp_payload_type_allowed(const struct rtp_header* hdr)
{
    auto payload_type = static_cast<Codec>(get_rtp_payload_type(hdr));

    if (allowed_codecs.count(payload_type)) {
        return true;
    }

    return false;
}

//----------------------------------------------------------------------

inline
bool is_protocol_RTP(const struct rtp_header* hdr, uint16_t packet_len)
{
    if (packet_len <= RTP_HEADER_MIN_LEN) {
        return false;
    }

    if (get_rtp_version(hdr) != RTP_VERSION) {
        return false;
    }

    return true;
}


#endif // __RTP_MEDIA_STREAM_PROCESSING_HPP__
