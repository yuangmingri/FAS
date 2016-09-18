#include "network_packet.hpp"



uint8_t* NetworkPacket::data()
{
    return data_.data();
}

//----------------------------------------------------------------------

const uint8_t* NetworkPacket::data() const
{
    return data_.data();
}

//----------------------------------------------------------------------

void NetworkPacket::flush()
{
    src_ = 0;
    dst_ = 0;
    sport_ = 0;
    dport_ = 0;
    proto_rtp_ = false;
    proto_sip_ = false;
    proto_unknown_ = true;
    rtp_meta_.flush();
    sip_meta_.flush();
}

//----------------------------------------------------------------------

uint32_t NetworkPacket::src() const
{
    return src_;
}

//----------------------------------------------------------------------

uint32_t NetworkPacket::dst() const
{
    return dst_;
}

//----------------------------------------------------------------------

uint16_t NetworkPacket::sport() const
{
    return sport_;
}

//----------------------------------------------------------------------

uint16_t NetworkPacket::dport() const
{
    return dport_;
}

//----------------------------------------------------------------------

const uint8_t* NetworkPacket::payload_ptr() const
{
    return payload_ptr_;
}

//----------------------------------------------------------------------

uint16_t NetworkPacket::payload_len() const
{
    return payload_len_;
}

//----------------------------------------------------------------------

bool NetworkPacket::proto_rtp() const
{
    return proto_rtp_;
}

//----------------------------------------------------------------------

bool NetworkPacket::proto_sip() const
{
    return proto_sip_;
}

//----------------------------------------------------------------------

bool NetworkPacket::proto_unknown() const
{
    return proto_unknown_;
}

//----------------------------------------------------------------------

RTPMetadata* NetworkPacket::rtp_meta()
{
    return &rtp_meta_;
}

//----------------------------------------------------------------------

const RTPMetadata* NetworkPacket::rtp_meta() const
{
    return &rtp_meta_;
}

//----------------------------------------------------------------------

SIPMetadata* NetworkPacket::sip_meta()
{
    return &sip_meta_;
}

//----------------------------------------------------------------------

const SIPMetadata* NetworkPacket::sip_meta() const
{
    return &sip_meta_;
}

//----------------------------------------------------------------------

void NetworkPacket::set_src(uint32_t src)
{
    src_ = src;
}

//----------------------------------------------------------------------

void NetworkPacket::set_dst(uint32_t dst)
{
    dst_ = dst;
}

//----------------------------------------------------------------------

void NetworkPacket::set_sport(uint16_t sport)
{
    sport_ = sport;
}

//----------------------------------------------------------------------

void NetworkPacket::set_dport(uint16_t dport)
{
    dport_ = dport;
}

//----------------------------------------------------------------------

void NetworkPacket::set_payload_ptr(const uint8_t* payload_ptr)
{
    payload_ptr_ = payload_ptr;
}

//----------------------------------------------------------------------

void NetworkPacket::set_payload_len(uint16_t payload_len)
{
    payload_len_ = payload_len;
}

//----------------------------------------------------------------------

void NetworkPacket::set_proto_rtp()
{
    proto_rtp_ = true;
    proto_sip_ = false;
    proto_unknown_ = false;
}

//----------------------------------------------------------------------

void NetworkPacket::set_proto_sip()
{
    proto_rtp_ = false;
    proto_sip_ = true;
    proto_unknown_ = false;
}

//----------------------------------------------------------------------

void NetworkPacket::set_proto_unknown()
{
    proto_rtp_ = false;
    proto_sip_ = false;
    proto_unknown_ = true;
}

//----------------------------------------------------------------------

uint16_t NetworkPacket::size() const
{
    return size_;
}

//----------------------------------------------------------------------

void NetworkPacket::set_size(uint16_t size)
{
    if (size > NetworkPacket::length) {
        return;
    }

    size_ = size;
}

