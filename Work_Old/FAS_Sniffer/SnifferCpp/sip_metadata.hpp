#ifndef __SIP_METADATA_HPP__
#define __SIP_METADATA_HPP__

#include <array>



class SIPMetadata
{
    public:
        enum class RequestMethod: uint8_t {UNDEFINED, BYE, CANCEL, INVITE};

        void flush();
        uint32_t address() const;
        uint16_t audio_port() const;
        const char* call_id() const;
        const char* from() const;
        const char* to() const;
        uint16_t response_code() const;
        RequestMethod request_method() const;
        void set_address(uint32_t);
        void set_audio_port(uint16_t);
        void set_call_id(const std::string&);
        void set_from(const std::string&);
        void set_to(const std::string&);
        void set_response_code(uint16_t);
        void set_request_method(RequestMethod);
    private:
        uint32_t address_;
        uint16_t audio_port_;
        uint16_t response_code_;
        RequestMethod request_method_;
        std::array<uint8_t, 256> call_id_;
        std::array<uint8_t, 256> from_;
        std::array<uint8_t, 256> to_;
};


#endif // __SIP_METADATA_HPP__
