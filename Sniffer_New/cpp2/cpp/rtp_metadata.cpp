#include "rtp_metadata.hpp"
#include <iostream>



void RTPMetadata::flush()
{
    id_ = 0;
    codec_ = Codec::UNDEFINED;
    payload_ptr_ = nullptr;
    payload_len_ = 0;
}

//----------------------------------------------------------------------

uint32_t RTPMetadata::id() const
{
    return id_;
}

//----------------------------------------------------------------------

uint16_t RTPMetadata::sport() const
{
    return sport_;
}

//----------------------------------------------------------------------

uint16_t RTPMetadata::dport() const
{
    return dport_;
}

//----------------------------------------------------------------------

uint64_t RTPMetadata::unique_id() const
{
    uint64_t unique_id = 0;
    uint64_t stream_id = id();
    uint32_t stream_sport = sport();
    uint32_t stream_dport = dport();

    unique_id = unique_id | stream_id << 32;
    unique_id = unique_id | stream_sport << 16;
    unique_id = unique_id | stream_dport;

    return unique_id;
}

//----------------------------------------------------------------------

uint16_t RTPMetadata::sequence_number() const
{
    return sequence_number_;
}

//----------------------------------------------------------------------

Codec RTPMetadata::codec() const
{
    return codec_;
}

//----------------------------------------------------------------------

uint8_t* RTPMetadata::payload_ptr() const
{
    return payload_ptr_;
}

//----------------------------------------------------------------------

uint16_t RTPMetadata::payload_len() const
{
    return payload_len_;
}

//----------------------------------------------------------------------

void RTPMetadata::set_id(uint32_t id)
{
    id_ = id;
}

//----------------------------------------------------------------------

void RTPMetadata::set_sport(uint16_t sport)
{
    sport_ = sport;
}

//----------------------------------------------------------------------

void RTPMetadata::set_dport(uint16_t dport)
{
    dport_ = dport;
}

//----------------------------------------------------------------------

void RTPMetadata::set_sequence_number(uint16_t sequence_number)
{
    sequence_number_ = sequence_number;
}

//----------------------------------------------------------------------

void RTPMetadata::set_codec(Codec codec)
{
    codec_ = codec;
}

//----------------------------------------------------------------------

void RTPMetadata::set_payload_ptr(const uint8_t* payload_ptr)
{
    payload_ptr_ = const_cast<uint8_t*>(payload_ptr);
}

//----------------------------------------------------------------------

void RTPMetadata::set_payload_len(uint16_t payload_len)
{
    payload_len_ = payload_len;
}
