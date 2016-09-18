#include "process_flusher.hpp"
#include <thread>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include "include.hpp"



using boost::interprocess::interprocess_mutex;
using boost::interprocess::scoped_lock;
using boost::interprocess::try_to_lock;


namespace flusher {


uint32_t current_packet_cnt = 0;
uint16_t current_segment_cnt = 0;
struct shared_memory_segment* current_segment;
static std::deque<struct shared_memory_segment*> segments;
scoped_lock<interprocess_mutex> segment_lock;


void acquire_segment()
{
    uint16_t attempt = 0;

    while (true) {
        if (current_segment_cnt >= segments.size()) {
            current_segment_cnt = 0;
        }

        auto segment = segments.at(current_segment_cnt);

        scoped_lock<interprocess_mutex> lck(segment->mtx, try_to_lock);
        if (lck) {
            // looking only for unflushed or assembled network packets
            if (!segment->network_packets_flushed && \
                !segment->network_packets_captured && \
                !segment->network_packets_processed) {
                //std::cout << __func__ << " flusher shm" << current_segment_cnt << std::endl;

                current_segment = segment;
                segment_lock = std::move(lck);

                break;
            } else {
                lck.unlock();
            }
        }

        ++current_segment_cnt;

        if (++attempt >= segments.size()) {
            attempt = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    current_packet_cnt = 0;
}

//----------------------------------------------------------------------

void release_segment()
{
    current_segment->network_packets_assembled = false;
    current_segment->network_packets_flushed = true;

    segment_lock.unlock();

    current_segment = nullptr;
    ++current_segment_cnt;
}

//----------------------------------------------------------------------

void process_segment()
{
    for (uint32_t i = 0; i < current_segment->data.size(); ++i) {
        auto& packet = current_segment->data.at(i);

        packet.flush();
    }
}


} // namespace flusher


void process_flusher()
{
    using namespace flusher;

    std::cout << __func__ << " " << getpid() << std::endl;

    // wait for all processes to be started
    std::this_thread::sleep_for(std::chrono::seconds(1));

    segments = get_shared_memory_segments();
    if (segments.empty()) {
        exit_nicely();
    }

    while (true) {
        acquire_segment();
        process_segment();
        release_segment();
    }
}

