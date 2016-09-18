#ifndef __PROCESS_ASSEMBLER_HPP__
#define __PROCESS_ASSEMBLER_HPP__

#include <future>
#include <memory>



// forward declaration
class Call;
class NetworkPacket;
class RTPMediaStream;
class SIPHeader;


void process_assembler();


namespace assembler {


void acquire_segment();

//----------------------------------------------------------------------

void release_segment();

//----------------------------------------------------------------------

void process_segment();

//----------------------------------------------------------------------

void handle_async_sip_header_output();

//----------------------------------------------------------------------

void handle_async_media_stream_output();

//----------------------------------------------------------------------

void handle_network_packet(uint64_t, const NetworkPacket&);

//----------------------------------------------------------------------

void analyze_sip(uint64_t, const NetworkPacket&);

//----------------------------------------------------------------------

void analyze_sip_call(uint64_t, const NetworkPacket&);

//----------------------------------------------------------------------

void analyze_sip_header(uint64_t, const NetworkPacket&);

//----------------------------------------------------------------------

void analyze_rtp(uint64_t, const NetworkPacket&);

//----------------------------------------------------------------------

void save_call(const std::string&, Call*);

//----------------------------------------------------------------------

void save_media_stream(const std::string&, const RTPMediaStream*);

//----------------------------------------------------------------------

void async_save_header(std::unique_ptr<SIPHeader>);

//----------------------------------------------------------------------

void async_save_media_stream(const std::string&, std::unique_ptr<RTPMediaStream>);

//----------------------------------------------------------------------

void save_sip_header(SIPHeader*);

//----------------------------------------------------------------------

void make_clean(uint64_t, std::future<bool>&, std::future<bool>&);

//----------------------------------------------------------------------

void make_clean_rtp_streams(uint64_t, std::future<bool>&);

//----------------------------------------------------------------------

void make_clean_sip_headers(uint64_t, std::future<bool>&);

//----------------------------------------------------------------------

std::unique_ptr<SIPHeader> make_header(uint64_t, const char*);

//----------------------------------------------------------------------

std::unique_ptr<RTPMediaStream> make_stream(uint64_t, const NetworkPacket&);

//----------------------------------------------------------------------

void cache_header(std::unique_ptr<SIPHeader>&&);

//----------------------------------------------------------------------

void cache_stream(std::unique_ptr<RTPMediaStream>&&);

//----------------------------------------------------------------------

bool is_write_ready(const std::string&, uint32_t, uint32_t);

//----------------------------------------------------------------------

std::future<bool> is_write_ready_rtp();

//----------------------------------------------------------------------

std::future<bool> is_write_ready_sip();

//----------------------------------------------------------------------

std::unique_ptr<RTPMediaStream> get_media_stream(uint32_t, uint16_t);


} // namespace assembler


#endif // __PROCESS_ASSEMBLER_HPP__
