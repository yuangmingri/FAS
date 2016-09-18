#include "call.hpp"
#include <iostream>



extern uint16_t CONFIG_CAPTURE_DURATION;


Call::Call(uint64_t now): callee_ip_(0), caller_ip_(0), ringtone_ip_(0),
    callee_port_(0), caller_port_(0), ringtone_port_(0), ringtone_stream_(nullptr)
{
    created_ = now;
}

//----------------------------------------------------------------------

uint32_t Call::callee_ip() const
{
    return callee_ip_;
}

//----------------------------------------------------------------------

uint32_t Call::caller_ip() const
{
    return caller_ip_;
}

//----------------------------------------------------------------------

uint32_t Call::ringtone_ip() const
{
    return ringtone_ip_;
}

//----------------------------------------------------------------------

uint16_t Call::callee_port() const
{
    return callee_port_;
}

//----------------------------------------------------------------------

uint16_t Call::caller_port() const
{
    return caller_port_;
}

//----------------------------------------------------------------------

uint16_t Call::ringtone_port() const
{
    return ringtone_port_;
}

//----------------------------------------------------------------------

std::unique_ptr<RTPMediaStream> Call::ringtone_stream()
{
    if (ringtone_stream_) {
        return std::move(ringtone_stream_);
    }

    return nullptr;
}

//----------------------------------------------------------------------

bool Call::ready(uint64_t now) const
{
    auto seconds = now - created_;
    if (seconds >= CONFIG_CAPTURE_DURATION * 5) {
        return true;
    }

    return false;
}

//----------------------------------------------------------------------

void Call::set_callee(uint32_t ip, uint16_t port)
{
    //std::cout << __func__ << " " << get_ip_str(ip) << ":" << port << std::endl;


    if (!callee_ip_ || callee_port_) {
        callee_ip_ = ip;
        callee_port_ = port;
    }
}

//----------------------------------------------------------------------

void Call::set_caller(uint32_t ip, uint16_t port)
{
    //std::cout << __func__ << " " << get_ip_str(ip) << ":" << port << std::endl;


    if (!caller_ip_ || !caller_port_) {
        caller_ip_ = ip;
        caller_port_ = port;
    }
}

//----------------------------------------------------------------------

void Call::set_ringtone(uint32_t ip, uint16_t port)
{
    //std::cout << __func__ << " " << get_ip_str(ip) << ":" << port << std::endl;


    if (!ringtone_ip_ || !ringtone_port_) {
        ringtone_ip_ = ip;
        ringtone_port_ = port;
    }
}

//----------------------------------------------------------------------

void Call::set_ringtone_stream(std::unique_ptr<RTPMediaStream> stream)
{
    //std::cout << __func__ << std::endl;

    ringtone_stream_ = std::move(stream);
}
