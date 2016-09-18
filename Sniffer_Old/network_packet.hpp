#ifndef __NETWORK_PACKET_HPP__
#define __NETWORK_PACKET_HPP__

#include <array>
#include "rtp_metadata.hpp"
#include "sip_metadata.hpp"



class NetworkPacket
{
    public:
        static constexpr uint16_t length = 1518;

        uint8_t* data();
        const uint8_t* data() const;
        void flush();

        uint32_t src() const;
        uint32_t dst() const;
        uint16_t sport() const;
        uint16_t dport() const;
        const uint8_t* payload_ptr() const;
        uint16_t payload_len() const;
        bool proto_rtp() const;
        bool proto_sip() const;
        bool proto_unknown() const;
        RTPMetadata* rtp_meta();
        const RTPMetadata* rtp_meta() const;
        SIPMetadata* sip_meta();
        const SIPMetadata* sip_meta() const;
        uint16_t size() const;

        void set_src(uint32_t);
        void set_dst(uint32_t);
        void set_sport(uint16_t);
        void set_dport(uint16_t);
        void set_payload_ptr(const uint8_t*);
        void set_payload_len(uint16_t);
        void set_proto_rtp();
        void set_proto_sip();
        void set_proto_unknown();
        void set_size(uint16_t);
    private:
        uint32_t src_ = 0;
        uint32_t dst_ = 0;
        uint16_t sport_ = 0;
        uint16_t dport_ = 0;
        bool proto_rtp_ = false;
        bool proto_sip_ = false;
        bool proto_unknown_ = true;
        const uint8_t* payload_ptr_ = nullptr;
        uint16_t payload_len_ = 0;
        RTPMetadata rtp_meta_;
        SIPMetadata sip_meta_;
        std::array<uint8_t, NetworkPacket::length> data_;
        uint16_t size_ = 0;
};


#endif // __NETWORK_PACKET_HPP__
