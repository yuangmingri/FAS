#ifndef __PROCESS_CAPTURE_HPP__
#define __PROCESS_CAPTURE_HPP__

#include <string>
#include <pcap.h>



void process_capture();

//----------------------------------------------------------------------

void start_capture(std::string, std::string);


namespace capture {

void acquire_segment();

//----------------------------------------------------------------------

void release_segment();

//----------------------------------------------------------------------

void packet_captured(uint8_t*, const struct pcap_pkthdr*, const uint8_t*);

} // namespace capture


#endif // __PROCESS_CAPTURE_HPP__
