#ifndef __CALL_HPP__
#define __CALL_HPP__

#include <chrono>
#include <memory>
#include "rtp_media_stream.hpp"



class Call
{
    public:
        Call(uint64_t);

        uint32_t callee_ip() const;
        uint32_t caller_ip() const;
        uint32_t ringtone_ip() const;
        uint16_t callee_port() const;
        uint16_t caller_port() const;
        uint16_t ringtone_port() const;
        std::unique_ptr<RTPMediaStream> ringtone_stream();
        bool ready(uint64_t) const;
        void set_callee(uint32_t, uint16_t);
        void set_caller(uint32_t, uint16_t);
        void set_ringtone(uint32_t, uint16_t);
        void set_ringtone_stream(std::unique_ptr<RTPMediaStream>);
    private:
        uint32_t callee_ip_;
        uint32_t caller_ip_;
        uint32_t ringtone_ip_;
        uint16_t callee_port_;
        uint16_t caller_port_;
        uint16_t ringtone_port_;
        std::unique_ptr<RTPMediaStream> ringtone_stream_;
        uint64_t created_;
};


#endif // __CALL_HPP__
