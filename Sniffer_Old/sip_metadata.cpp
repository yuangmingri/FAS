#include "sip_metadata.hpp"



void SIPMetadata::flush()
{
    address_ = 0;
    audio_port_ = 0;
    response_code_ = 0;
    request_method_ = RequestMethod::UNDEFINED;
    call_id_.fill(0);
    from_.fill(0);
    to_.fill(0);
}

//----------------------------------------------------------------------

uint32_t SIPMetadata::address() const
{
    return address_;
}

//----------------------------------------------------------------------

uint16_t SIPMetadata::audio_port() const
{
    return audio_port_;
}

//----------------------------------------------------------------------

const char* SIPMetadata::call_id() const
{
    return reinterpret_cast<const char*>(call_id_.data());
}

//----------------------------------------------------------------------

const char* SIPMetadata::from() const
{
    return reinterpret_cast<const char*>(from_.data());
}

//----------------------------------------------------------------------

const char* SIPMetadata::to() const
{
    return reinterpret_cast<const char*>(to_.data());
}

//----------------------------------------------------------------------

uint16_t SIPMetadata::response_code() const
{
    return response_code_;
}

//----------------------------------------------------------------------

SIPMetadata::RequestMethod SIPMetadata::request_method() const
{
    return request_method_;
}

//----------------------------------------------------------------------

void SIPMetadata::set_address(uint32_t address)
{
    address_ = address;
}

//----------------------------------------------------------------------

void SIPMetadata::set_audio_port(uint16_t audio_port)
{
    audio_port_ = audio_port;
}

//----------------------------------------------------------------------

void SIPMetadata::set_call_id(const std::string& call_id)
{
    if (call_id.size() < call_id_.size()) {
        std::copy(call_id.begin(), call_id.end(), call_id_.begin());
        // ensure that null terminated
        call_id_.at(call_id.size() + 1) = 0;
    }
}

//----------------------------------------------------------------------

void SIPMetadata::set_from(const std::string& from)
{
    if (from.size() < from_.size()) {
        std::copy(from.begin(), from.end(), from_.begin());
        // ensure that null terminated
        from_.at(from.size() + 1) = 0;
    }
}

//----------------------------------------------------------------------

void SIPMetadata::set_to(const std::string& to)
{
    if (to.size() < to_.size()) {
        std::copy(to.begin(), to.end(), to_.begin());
        // ensure that null terminated
        to_.at(to.size() + 1) = 0;
    }
}

//----------------------------------------------------------------------

void SIPMetadata::set_response_code(uint16_t response_code)
{
    response_code_ = response_code;
}

//----------------------------------------------------------------------

void SIPMetadata::set_request_method(SIPMetadata::RequestMethod request_method)
{
    request_method_ = request_method;
}


