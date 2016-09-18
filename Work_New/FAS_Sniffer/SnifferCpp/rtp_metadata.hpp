#ifndef __RTP_METADATA_HPP__
#define __RTP_METADATA_HPP__

#include "codec.hpp"



class RTPMetadata
{
    public:
        void flush();
        uint32_t id() const;
        uint16_t sport() const;
        uint16_t dport() const;
        uint64_t unique_id() const;
        uint16_t sequence_number() const;
        Codec codec() const;
        uint8_t* payload_ptr() const;
        uint16_t payload_len() const;
        void set_id(uint32_t);
        void set_sport(uint16_t);
        void set_dport(uint16_t);
        void set_sequence_number(uint16_t);
        void set_codec(Codec);
        void set_payload_ptr(const uint8_t*);
        void set_payload_len(uint16_t);
    private:
        uint32_t id_;
        uint16_t sport_;
        uint16_t dport_;
        uint16_t sequence_number_;
        Codec codec_;
        uint8_t* payload_ptr_;
        uint16_t payload_len_;
};


#endif // __RTP_METADATA_HPP__
