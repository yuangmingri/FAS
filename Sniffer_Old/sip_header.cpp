#include "sip_header.hpp"
#include <iostream>



extern uint16_t CONFIG_SIP_HEADER_TIMEOUT;


SIPHeader::SIPHeader(uint64_t now, std::string call_id)
{
    call_id_ = std::move(call_id);

    for (uint16_t i = 0; i < SIPHeader::cached; ++i) {
        auto packet = std::make_unique<NetworkPacket>();
        data_.emplace_back(std::move(packet));
    }


    created_ = now;
}

//----------------------------------------------------------------------

SIPHeader::~SIPHeader()
{
    //std::cout << __func__ << " " << call_id_ << std::endl;
}

//----------------------------------------------------------------------

void SIPHeader::add(uint64_t now, const NetworkPacket& received_packet)
{
    //std::cout << __func__ << std::endl;

    NetworkPacket* packet = nullptr;

    if (network_packet_cnt() < SIPHeader::cached) {
        packet = data_.at(network_packet_cnt()).get();
    } else {
        auto packet_uptr = std::make_unique<NetworkPacket>();
        packet = packet_uptr.get();
        data_.emplace_back(std::move(packet_uptr));
    }
    ++network_packet_cnt_;


    std::memcpy(packet->data(), received_packet.data(), received_packet.size());
    packet->set_size(received_packet.size());


    update_last_activity(now);
}

//----------------------------------------------------------------------

const std::string& SIPHeader::call_id() const
{
    return call_id_;
}

//----------------------------------------------------------------------

const char* SIPHeader::call_id_cstr() const
{
    return call_id_.c_str();
}

//----------------------------------------------------------------------

uint64_t SIPHeader::created() const
{
    return created_;
}

//----------------------------------------------------------------------

const std::vector<std::unique_ptr<NetworkPacket>>& SIPHeader::data() const
{
    return data_;
}

//----------------------------------------------------------------------

void SIPHeader::flush()
{
    if (network_packet_cnt() > (SIPHeader::cached * 2)) {
        data_.resize(SIPHeader::cached);
    }

    network_packet_cnt_ = 0;
}

//----------------------------------------------------------------------

uint16_t SIPHeader::network_packet_cnt() const
{
    return network_packet_cnt_;
}

//----------------------------------------------------------------------

void SIPHeader::set_call_finished(uint64_t now)
{
    call_finished_ = true;
    call_finished_ts_ = now;
}

//----------------------------------------------------------------------

bool SIPHeader::ready(uint64_t now) const
{
    if (call_finished_) {
        auto minutes = (now - call_finished_ts_) / 60;
        if (minutes >= 1) {
            return true;
        }

        return false;
    }

    auto minutes = (now - last_activity_) / 60;
    if (minutes >= CONFIG_SIP_HEADER_TIMEOUT) {
        return true;
    }

    return false;
}

//----------------------------------------------------------------------

void SIPHeader::reset(std::string call_id)
{
    call_id_ = std::move(call_id);
    call_finished_ = false;
}

//----------------------------------------------------------------------

void SIPHeader::update_last_activity(uint64_t now)
{
    last_activity_ = now;
}
