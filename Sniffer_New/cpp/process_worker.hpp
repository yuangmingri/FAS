#ifndef __PROCESS_WORKER_HPP__
#define __PROCESS_WORKER_HPP__

#include "network_packet.hpp"



void process_worker();


namespace worker {


void acquire_segment();

//----------------------------------------------------------------------

void release_segment();

//----------------------------------------------------------------------

void process_segment();

//----------------------------------------------------------------------

void analyze(NetworkPacket&);

//----------------------------------------------------------------------

void analyze_sip_header(NetworkPacket&, const uint8_t*, uint16_t);

//----------------------------------------------------------------------

void analyze_rtp_header(NetworkPacket&, const uint8_t*, uint16_t);


} // namespace worker


#endif // __PROCESS_WORKER_HPP__
