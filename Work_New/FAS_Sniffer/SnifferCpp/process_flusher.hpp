#ifndef __PROCESS_FLUSHER_HPP__
#define __PROCESS_FLUSHER_HPP__

#include "network_packet.hpp"



void process_flusher();


namespace flusher {


void acquire_segment();

//----------------------------------------------------------------------

void release_segment();

//----------------------------------------------------------------------

void process_segment();


} // namespace flusher


#endif // __PROCESS_FLUSHER_HPP__
