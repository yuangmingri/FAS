#include "process_capture.hpp"
#include <thread>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include "include.hpp"



using boost::interprocess::interprocess_mutex;
using boost::interprocess::scoped_lock;
using boost::interprocess::try_to_lock;


extern std::string CONFIG_CAPTURE_OPTIONS;
extern std::string CONFIG_NETWORK_INTERFACE;
extern uint32_t CONFIG_PCAP_BUFFER_SIZE;
extern pcap_t* pcap_handle;


namespace capture {


uint32_t current_packet_cnt = 0;
uint16_t current_segment_cnt = 0;
struct shared_memory_segment* current_segment = nullptr;
static std::deque<struct shared_memory_segment*> segments;
constexpr uint32_t shm_segment_packet_cnt = shared_memory_segment::network_packet_count;
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
            // looking only for processed or unprocessed segments
            if (segment->network_packets_flushed && \
                !segment->network_packets_captured && \
                !segment->network_packets_processed && \
                !segment->network_packets_assembled) {
                //std::cout << __func__ << " capture shm" << current_segment_cnt << std::endl;

                current_segment = segment;
                segment_lock = std::move(lck);

                break;
            } else {
                lck.unlock();
            }
        }

        ++current_segment_cnt;

        if (++attempt >= segments.size()) {
            //std::cout << "[!] unable to acquire segment in capture!" << std::endl;
            attempt = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    current_packet_cnt = 0;
}

//----------------------------------------------------------------------

void release_segment()
{
    current_segment->network_packets_flushed = false;
    current_segment->network_packets_captured = true;

    segment_lock.unlock();

    current_segment = nullptr;
    ++current_segment_cnt;


    struct pcap_stat stat;

    if (pcap_stats(pcap_handle, &stat) == 0) {
        std::cout << "\r[i] ";
        std::cout << " recv " << stat.ps_recv;
        std::cout << " | drop " << stat.ps_drop;
        std::cout << " | ifdrop " << stat.ps_ifdrop;
        std::cout << " ";
        std::cout << std::flush;
    }
}

//----------------------------------------------------------------------

void packet_captured(uint8_t*, const struct pcap_pkthdr* hdr, const uint8_t* packet)
{
    if ((hdr->len > ETHERNET_HEADER_LEN) && (hdr->len <= NetworkPacket::length)) {
        // common case
        if (current_segment && (current_packet_cnt < shm_segment_packet_cnt)) {
            const uint8_t* payload_ptr = packet;
            const uint16_t payload_len = hdr->len;            

            auto& packet = current_segment->data.at(current_packet_cnt);
            std::memcpy(packet.data(), payload_ptr, payload_len);
            packet.set_size(payload_len);

            ++current_packet_cnt;

            return;
        } else if (current_packet_cnt >= shm_segment_packet_cnt) {
            //std::cout << __func__ << " current_packet_cnt >= shm_segment_packet_cnt -> " << current_packet_cnt << " : " << shm_segment_packet_cnt << std::endl;

            release_segment();
            acquire_segment();
        } else if (!current_segment) {
            //std::cout << __func__ << " !current_segment" << std::endl;

            acquire_segment();
        } else {
            std::cout << __func__ << " unknown" << std::endl;
        }

        // if segment was changed -> recall self
        packet_captured(nullptr, hdr, packet);
    }
}


} // namespace capture


//----------------------------------------------------------------------

void start_capture(std::string filter_options, std::string device_name = "")
{
    using capture::packet_captured;

    constexpr uint16_t SNAP_LEN = 1518;
    constexpr uint16_t PROMISC_MODE = 1;

    char* dev = nullptr;
    pcap_t* handle = nullptr;
    std::array<char, PCAP_ERRBUF_SIZE> errbuf;
    struct bpf_program fp;
    bpf_u_int32 mask;
    bpf_u_int32 net;

    std::cerr << "Calling start_capture" << std::endl;

    if (device_name.length()) {
        dev = &device_name.at(0);
    } else {
        dev = pcap_lookupdev(errbuf.data());
        if (!dev) {
            std::cerr << "Unable to find default device" << std::endl;
            exit_nicely();
        }
    }

    if (pcap_lookupnet(dev, &net, &mask, errbuf.data()) == -1) {
        net = 0;
        mask = 0;
    }


    handle = pcap_create(dev, errbuf.data());
    if (!handle) {
        std::cerr << "Unable to create pcap handle for device '" << dev << "' (" << pcap_geterr(handle) << ")." << std::endl;
        exit_nicely();
    }

    if (pcap_set_snaplen(handle, SNAP_LEN) != 0) {
        std::cerr << "Unable to set snapshot len on device '" << dev << "' (" << pcap_geterr(handle) << ")." << std::endl;
        exit_nicely();
    }

    if (pcap_set_promisc(handle, PROMISC_MODE) != 0) {
        std::cerr << "Unable to activate promisc mode on device '" << dev << "' (" << pcap_geterr(handle) << ")." << std::endl;
        exit_nicely();
    }

    if (pcap_set_buffer_size(handle, CONFIG_PCAP_BUFFER_SIZE) != 0) {
        std::cerr << "Unable to set buffer size on device '" << dev << "' (" << pcap_geterr(handle) << ")." << std::endl;
        exit_nicely();
    }

    if (pcap_activate(handle) != 0) {
        std::cerr << "Unable to activate capture on device '" << dev << "' (" << pcap_geterr(handle) << ")." << std::endl;
        exit_nicely();
    }

    if (pcap_datalink(handle) != DLT_EN10MB) {
        std::cerr << "Unable to capture on non ethernet device '" << dev << "' (" << pcap_geterr(handle) << ")." << std::endl;
        exit_nicely();
    }

    if (pcap_compile(handle, &fp, filter_options.c_str(), 0, net) == -1) {
        std::cerr << "Unable to parse filter expression: '" << filter_options << "' (" << pcap_geterr(handle) << ")." << std::endl;
        exit_nicely();
    }

    if (pcap_setfilter(handle, &fp) == -1) {
        std::cerr << "Unable to install parse filter (" << pcap_geterr(handle) << ")." << std::endl;
        exit_nicely();
    }

    pcap_handle = handle;
    pcap_loop(handle, -1, packet_captured, nullptr);

    pcap_freecode(&fp);
    pcap_close(handle);
}

//----------------------------------------------------------------------

void process_capture()
{
    using namespace capture;

    std::cout << __func__ << " " << getpid() << std::endl;

    // wait for all processes to be started
    std::this_thread::sleep_for(std::chrono::seconds(1));

    segments = get_shared_memory_segments();
    if (segments.empty()) {
        exit_nicely();
    }

    start_capture(CONFIG_CAPTURE_OPTIONS, CONFIG_NETWORK_INTERFACE);
}
