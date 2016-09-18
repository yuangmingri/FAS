#ifndef __INCLUDE_HPP__
#define __INCLUDE_HPP__

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <array>
#include <bitset>
#include <chrono>
#include <deque>
#include <iostream>
#include <random>
#include <sstream>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include "network_packet.hpp"



using boost::interprocess::open_only;
using boost::interprocess::interprocess_mutex;
using boost::interprocess::mapped_region;
using boost::interprocess::read_write;
using boost::interprocess::shared_memory_object;


constexpr uint16_t RTP_VERSION         = 2;
constexpr uint16_t RTP_HEADER_MIN_LEN  = 12;
constexpr uint16_t ETHERNET_HEADER_LEN = 14;


extern uint16_t CONFIG_SHM_COUNT;
extern const uint32_t CONFIG_SHM_SEGMENT_PACKET_CNT;


struct rtp_header
{
    uint8_t control;
    uint8_t payload;
    uint16_t sequence_number;
    uint32_t timestamp;
    uint32_t sync_identifier;
};

//----------------------------------------------------------------------

struct media_file_header
{
    uint32_t src;
    uint32_t dst;
    uint16_t sport;
    uint16_t dport;
    uint32_t codec;
};

//----------------------------------------------------------------------

struct shm_remove
{
    shm_remove(std::string name): shm_name(name)
    {
	    shared_memory_object::remove(shm_name.c_str());
        std::cout << __func__ << shm_name << std::endl;
    };
    ~shm_remove()
    {
	    shared_memory_object::remove(shm_name.c_str());
        std::cout << __func__ << shm_name << std::endl;
    };
    std::string shm_name;
};

//----------------------------------------------------------------------

struct shared_memory_segment
{
    static constexpr uint32_t network_packet_count = 5120;

    interprocess_mutex mtx;
    bool network_packets_flushed = false;
    bool network_packets_captured = false;
    bool network_packets_processed = false;
    bool network_packets_assembled = false;
    std::array<NetworkPacket, shared_memory_segment::network_packet_count> data;
};

//----------------------------------------------------------------------

inline
std::deque<struct shared_memory_segment*> get_shared_memory_segments()
{
    std::deque<struct shared_memory_segment*> segments;
    static std::deque<std::shared_ptr<shared_memory_object>> shms;
    static std::deque<std::shared_ptr<mapped_region>> regions;

    try {
        for (uint16_t i = 0; i < CONFIG_SHM_COUNT; ++i) {
            std::stringstream out;
            out << "shm" << i;

            auto name = out.str();

            //std::cout << __func__ << " " << name << std::endl;


            auto shm = std::make_shared<shared_memory_object>(open_only, name.c_str(), read_write);
            auto region = std::make_shared<mapped_region>(*(shm), read_write);

            shms.push_back(shm);
            regions.push_back(region);

            auto segment = static_cast<struct shared_memory_segment*>(region->get_address());
            segments.push_back(segment);

            //shared_memory_object shm(open_only, name.c_str(), read_write);
            //mapped_region region(shm, read_write);

            //auto segment = static_cast<struct shared_memory_segment*>(region.get_address());
            //segments.push_back(segment);
        }
    } catch (...) {
        segments.clear();
    }

    return segments;
}

//----------------------------------------------------------------------

inline
std::string get_ip_str(const uint32_t ip)
{
    struct in_addr addr;
    memset(&addr, 0, sizeof(addr));

    addr.s_addr = htonl(ip);

    return inet_ntoa(addr);
}

//----------------------------------------------------------------------

inline
uint32_t get_ip_from_str(const std::string ip_raw)
{
    std::string ip = ip_raw;

    auto idx = ip.find("/");
    if (idx != std::string::npos) {
        ip = ip.substr(0, idx);
    }

    struct sockaddr_in addr;
    inet_pton(AF_INET, ip.c_str(), &(addr.sin_addr));

    return ntohl(addr.sin_addr.s_addr);
}

//----------------------------------------------------------------------

inline
int32_t random_number(int32_t from, int32_t to)
{
    std::uniform_int_distribution<int32_t> dist(from, to);

    std::random_device rd;
    std::mt19937 engine(rd());

    return dist(engine);
}

//----------------------------------------------------------------------

inline
std::string strf(const std::chrono::time_point<std::chrono::system_clock>& dt, const std::string& format_string)
{
    auto tt = std::chrono::system_clock::to_time_t(dt);

    std::array<char, 64> buffer;
    uint16_t rv = std::strftime(buffer.data(), buffer.size(), format_string.c_str(), std::gmtime(&tt));
    if (rv) {
        return std::string(buffer.begin(), buffer.begin() + rv);
    }

    return "";
}

//----------------------------------------------------------------------

inline
uint16_t get_terminal_width()
{
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

    return ws.ws_col;
}

//----------------------------------------------------------------------

inline
void print_line()
{
    auto width = get_terminal_width();
    std::string line(width, '-');
    std::cout << line << std::endl;
}

//----------------------------------------------------------------------

inline
void exit_nicely(int32_t code = 0)
{
    kill(SIGINT, getppid());
    exit(code);
}


#endif // __INCLUDE_HPP__
