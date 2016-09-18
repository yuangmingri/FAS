#ifndef __SIP_HEADER_HPP__
#define __SIP_HEADER_HPP__

#include <cstdint>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include "include.hpp"
#include "network_packet.hpp"



class SIPHeader
{
    public:
        static constexpr uint16_t cached = 8;

        SIPHeader(uint64_t, std::string);
        ~SIPHeader();

        void add(uint64_t, const NetworkPacket&);
        const std::string& call_id() const;
        const char* call_id_cstr() const;
        uint64_t created() const;
        const std::vector<std::unique_ptr<NetworkPacket>>& data() const;
        void flush();
        uint16_t network_packet_cnt() const;
        void reset(std::string);
        void set_call_finished(uint64_t);
        bool ready(uint64_t) const;
    private:
        void update_last_activity(uint64_t);

        std::string call_id_;
        bool call_finished_ = false;
        uint16_t network_packet_cnt_ = 0;
        std::vector<std::unique_ptr<NetworkPacket>> data_;
        uint64_t created_;
        uint64_t call_finished_ts_;
        uint64_t last_activity_;
};


#endif // __SIP_HEADER_HPP__
