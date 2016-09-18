#include "process_assembler.hpp"
#include <ctime>
#include <deque>
#include <fstream>
#include <unordered_map>
#include <thread>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <pcap.h>
#include "call.hpp"
#include "include.hpp"
#include "rtp_media_stream.hpp"
#include "sip_header.hpp"
#include "sip_metadata.hpp"



using boost::interprocess::interprocess_mutex;
using boost::interprocess::scoped_lock;
using boost::interprocess::try_to_lock;


extern uint16_t CONFIG_CAPTURE_DURATION;
extern bool CONFIG_MODE_DUMP_RTP_STREAMS;
extern bool CONFIG_MODE_DUMP_SIP_HEADERS;
extern std::string CONFIG_RTP_DESTINATION_DIRECTORY;
extern std::string CONFIG_SIP_DESTINATION_DIRECTORY;
extern uint32_t CONFIG_RTP_DESTINATION_DIRECTORY_MAXSIZE;
extern uint32_t CONFIG_RTP_DESTINATION_DIRECTORY_MAX_FILE_CNT;
extern uint32_t CONFIG_SIP_DESTINATION_DIRECTORY_MAXSIZE;
extern uint32_t CONFIG_SIP_DESTINATION_DIRECTORY_MAX_FILE_CNT;
extern std::map<Codec, struct codec_details> codec_lookup_table;


namespace assembler {


uint32_t current_packet_cnt = 0;
uint16_t current_segment_cnt = 0;
struct shared_memory_segment* current_segment;
static std::deque<struct shared_memory_segment*> segments;
scoped_lock<interprocess_mutex> segment_lock;


std::unordered_map<std::string, std::unique_ptr<Call>> calls;
std::unordered_map<uint32_t, std::unique_ptr<RTPMediaStream>> media_streams;
std::deque<std::unique_ptr<RTPMediaStream>> media_streams_cached;
std::unordered_map<std::string, std::unique_ptr<SIPHeader>> sip_headers;
std::deque<std::unique_ptr<SIPHeader>> sip_headers_cached;
std::deque<std::future<std::unique_ptr<SIPHeader>>> async_header_io;
std::deque<std::future<std::unique_ptr<RTPMediaStream>>> async_media_stream_io;


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
            // looking only for unprocessed segments
            if (!segment->network_packets_flushed && \
                !segment->network_packets_captured && \
                segment->network_packets_processed && \
                !segment->network_packets_assembled) {
                //std::cout << __func__ << " assembler shm" << current_segment_cnt << std::endl;

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
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    current_packet_cnt = 0;
}

//----------------------------------------------------------------------

void release_segment()
{
    current_segment->network_packets_processed = false;
    current_segment->network_packets_assembled = true;

    segment_lock.unlock();

    current_segment = nullptr;
    ++current_segment_cnt;
}

//----------------------------------------------------------------------

void process_segment()
{
    handle_async_sip_header_output();
    handle_async_media_stream_output();

    auto write_ready_sip = is_write_ready_sip();
    auto write_ready_rtp = is_write_ready_rtp();

    auto now = time(0);

    for (uint32_t i = 0; i < current_segment->data.size(); ++i) {
        auto& packet = current_segment->data.at(i);

        if (!packet.proto_unknown()) {
            handle_network_packet(now, packet);
        }
    }

    make_clean(now, write_ready_sip, write_ready_rtp);
}

//----------------------------------------------------------------------

void handle_async_sip_header_output()
{
    // first handle only for valid futures
    for (auto itr = async_header_io.begin(); itr != async_header_io.end();) {
        auto& fut = *itr;

        if (fut.valid()) {
            auto header = fut.get();
            cache_header(std::move(header));

            itr = async_header_io.erase(itr);
        } else {
            ++itr;
        }
    }

    // next all other futures
    for (auto itr = async_header_io.begin(); itr != async_header_io.end();) {
        auto& fut = *itr;

        auto header = fut.get();
        cache_header(std::move(header));
    }

    async_header_io.clear();
}

//----------------------------------------------------------------------

void handle_async_media_stream_output()
{
    // first handle only for valid futures
    for (auto itr = async_media_stream_io.begin(); itr != async_media_stream_io.end();) {
        auto& fut = *itr;

        if (fut.valid()) {
            auto stream = fut.get();
            cache_stream(std::move(stream));

            itr = async_media_stream_io.erase(itr);
        } else {
            ++itr;
        }
    }

    // next all other futures
    for (auto itr = async_media_stream_io.begin(); itr != async_media_stream_io.end();) {
        auto& fut = *itr;

        auto stream = fut.get();
        cache_stream(std::move(stream));
    }

    async_media_stream_io.clear();
}

//----------------------------------------------------------------------

void handle_network_packet(uint64_t now, const NetworkPacket& packet)
{
    if (packet.proto_sip()) {
        analyze_sip(now, packet);
    } else if (packet.proto_rtp()) {
        if (CONFIG_MODE_DUMP_RTP_STREAMS) {
            analyze_rtp(now, packet);
        }
    }
}

//----------------------------------------------------------------------

void analyze_sip(uint64_t now, const NetworkPacket& packet)
{
    if (CONFIG_MODE_DUMP_SIP_HEADERS) {
        analyze_sip_header(now, packet);
    }
    if (CONFIG_MODE_DUMP_RTP_STREAMS) {
        analyze_sip_call(now, packet);
    }
}

//----------------------------------------------------------------------

void analyze_sip_call(uint64_t now, const NetworkPacket& packet)
{
    auto meta = packet.sip_meta();
    Call* call = nullptr;

    //std::cout << "Call " << meta->call_id() << std::endl;

    try {
        call = calls.at(meta->call_id()).get();
    } catch (const std::out_of_range&) {
        if (meta->request_method() != SIPMetadata::RequestMethod::INVITE) {
            return;
        }

        auto call_uptr = std::make_unique<Call>(now);
        call_uptr->set_caller(meta->address(), meta->audio_port());
        calls.emplace(meta->call_id(), std::move(call_uptr));

        return;
    }

    if ((meta->response_code() == 180) || (meta->response_code() == 183)) {
        call->set_ringtone(meta->address(), meta->audio_port());
    } else if (meta->response_code() == 200) {
        if (meta->address() && meta->audio_port()) {

            call->set_callee(meta->address(), meta->audio_port());

            auto ringtone_ip = call->ringtone_ip();
            auto ringtone_port = call->ringtone_port();

            if (ringtone_ip && ringtone_port) {
                auto ringtone_stream = get_media_stream(ringtone_ip, ringtone_port);
                if (ringtone_stream) {
                    call->set_ringtone_stream(std::move(ringtone_stream));
                }
            }
        }
    }
}

//----------------------------------------------------------------------

void analyze_sip_header(uint64_t now, const NetworkPacket& packet)
{
    auto meta = packet.sip_meta();
    SIPHeader* hdr = nullptr;

    try {
        hdr = sip_headers.at(meta->call_id()).get();
    } catch (const std::out_of_range&) {
        if (meta->request_method() != SIPMetadata::RequestMethod::INVITE) {
            return;
        }

        auto hdr_uptr = make_header(now, meta->call_id());
        hdr = hdr_uptr.get();
        sip_headers.emplace(meta->call_id(), std::move(hdr_uptr));
    }


    hdr->add(now, packet);


    if (meta->request_method() == SIPMetadata::RequestMethod::BYE || \
        meta->request_method() == SIPMetadata::RequestMethod::CANCEL) {
        hdr->set_call_finished(now);
    }
}

//----------------------------------------------------------------------

void analyze_rtp(uint64_t now, const NetworkPacket& packet)
{
    auto meta = packet.rtp_meta();
    RTPMediaStream* stream = nullptr;

    try {
        stream = media_streams.at(meta->unique_id()).get();
    } catch (const std::out_of_range&) {
        auto stream_uptr = make_stream(now, packet);
        stream = stream_uptr.get();
        media_streams[meta->unique_id()] = std::move(stream_uptr);
    }

    stream->add_frame(now, meta->sequence_number(), meta->payload_ptr(), meta->payload_len());
}

//----------------------------------------------------------------------

void save_call(const std::string& call_id, Call* call)
{
    auto callee_ip = call->callee_ip();
    auto caller_ip = call->caller_ip();
    auto ringtone_ip = call->ringtone_ip();
    auto callee_port = call->callee_port();
    auto caller_port = call->caller_port();
    auto ringtone_port = call->ringtone_port();


    if (caller_ip && caller_port) {
        auto caller_stream = get_media_stream(caller_ip, caller_port);
        if (caller_stream) {
            std::string filename = CONFIG_RTP_DESTINATION_DIRECTORY + "/" + call_id + "_caller.media";
            //save_media_stream(filename, caller_stream);
            //cache_stream(std::move(caller_stream));
            async_save_media_stream(filename, std::move(caller_stream));
        } else {
            //std::cout << "caller stream not found " << get_ip_str(caller_ip) << ":" << caller_port << std::endl;
        }
    }

    if (callee_ip && callee_port) {
        auto callee_stream = get_media_stream(callee_ip, callee_port);
        if (callee_stream) {
            std::string filename = CONFIG_RTP_DESTINATION_DIRECTORY + "/" + call_id + "_callee.media";
            //save_media_stream(filename, callee_stream);
            //cache_stream(std::move(callee_stream));
            async_save_media_stream(filename, std::move(callee_stream));
        } else {
            //std::cout << "callee stream not found " << get_ip_str(callee_ip) << ":" << callee_port << std::endl;
        }
    }

    if (ringtone_ip && ringtone_port) {
        auto ringtone_stream = call->ringtone_stream();
        if (!ringtone_stream) {
            ringtone_stream = get_media_stream(ringtone_ip, ringtone_port);
        }

        if (ringtone_stream) {
            std::string filename = CONFIG_RTP_DESTINATION_DIRECTORY + "/" + call_id + "_ringtone.media";
            //save_media_stream(filename, ringtone_stream);
            //cache_stream(std::move(ringtone_stream));
            async_save_media_stream(filename, std::move(ringtone_stream));
        } else {
            //std::cout << "ringtone stream not found " << get_ip_str(ringtone_ip) << ":" << ringtone_port << std::endl;
        }
    }
}

//----------------------------------------------------------------------

void save_media_stream(const std::string& filename, const RTPMediaStream* stream)
{
    struct media_file_header header;
    static constexpr uint16_t header_len = sizeof(header);
    header.src = stream->src();
    header.dst = stream->dst();
    header.sport = stream->sport();
    header.dport = stream->dport();
    header.codec = static_cast<uint32_t>(stream->codec());

    std::cout << ">>> saving " << filename << " " << get_ip_str(stream->src()) << ":" << stream->sport();
    std::cout << " -> " << get_ip_str(stream->dst()) << ":" << stream->dport() << std::endl;

    try {
        std::ofstream fdata(filename, std::ios::binary | std::ios::out);
        if (fdata.is_open()) {
            fdata.write(reinterpret_cast<const char*>(&header), header_len);

            auto frame_cnt = stream->frame_cnt();
            uint32_t saved_frame_cnt = 0;

            for (uint32_t i = 0; i < frame_cnt; ++i) {
                auto& frame = stream->frames().at(i);

                //std::cout << "<<< save sequence number " << frame->sequence_number() << std::endl;


                fdata.write(reinterpret_cast<const char*>(frame->data()), frame->data_len());

                ++saved_frame_cnt;
            }

            fdata.close();

            std::cout << "<<< saved frame cnt " << saved_frame_cnt << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "[!] unable to save media, reason " << e.what() << std::endl;
    }
}

//----------------------------------------------------------------------

void async_save_header(std::unique_ptr<SIPHeader> header)
{
    auto save_task = [header = std::move(header)]() mutable {
        save_sip_header(header.get());
        return std::move(header);
    };


    auto future = std::async(std::launch::async | std::launch::deferred, std::move(save_task));
    async_header_io.emplace_back(std::move(future));       
}

//----------------------------------------------------------------------

void async_save_media_stream(const std::string& filename, std::unique_ptr<RTPMediaStream> stream_uptr)
{
    auto stream = stream_uptr.get();
    stream_uptr.release();


    auto save_task = [filename = filename, stream = stream]() mutable {
        save_media_stream(filename, stream);
        return std::unique_ptr<RTPMediaStream>(stream);
    };


    try {
        auto future = std::async(std::launch::async, save_task);
        async_media_stream_io.emplace_back(std::move(future));
    } catch (const std::system_error&) {
        auto saved_stream_uptr = save_task();
        cache_stream(std::move(saved_stream_uptr));
    }
}

//----------------------------------------------------------------------

void save_sip_header(SIPHeader* hdr)
{
    std::string name = "";
    name += hdr->call_id();
    name += ".pcap";


    std::string headerfile = CONFIG_SIP_DESTINATION_DIRECTORY + "/" + name;


    static pcap_t* handler = pcap_open_dead(DLT_EN10MB, 65535);
    pcap_dumper_t* dumper = pcap_dump_open(handler, headerfile.c_str());
    if (dumper) {
        pcap_pkthdr h;

        for (uint16_t i = 0; i < hdr->network_packet_cnt(); ++i) {
            const auto& packet = hdr->data().at(i);

            h.caplen = packet->size();
            h.len = packet->size();

            pcap_dump(reinterpret_cast<u_char*>(dumper), &h, packet->data());
        }

        pcap_dump_close(dumper);
    } else {
        std::cerr << "[!] unable to save .pcap (destination directory doesn't exists)" << std::endl;
    }
}

//----------------------------------------------------------------------

void make_clean(uint64_t now, std::future<bool>& write_ready_future_sip, std::future<bool>& write_ready_future_rtp)
{
    if (CONFIG_MODE_DUMP_SIP_HEADERS) {
        make_clean_sip_headers(now, write_ready_future_sip);
    }
    if (CONFIG_MODE_DUMP_RTP_STREAMS) {
        make_clean_rtp_streams(now, write_ready_future_rtp);
    }
}

//----------------------------------------------------------------------

void make_clean_rtp_streams(uint64_t now, std::future<bool>& write_ready_future)
{
    auto write_ready = write_ready_future.get();


    for (auto itr = calls.begin(); itr != calls.end();) {
        auto& call = itr->second;

        if (call->ready(now)) {
            if (write_ready) {
                save_call(itr->first, call.get());
            }

            itr = calls.erase(itr);
        } else {
            ++itr;
        }
    }


    for (auto itr = media_streams.begin(); itr != media_streams.end();) {
        auto& stream = itr->second;

        if (stream->ready(now)) {
            auto ready_stream = std::move(itr->second);
            cache_stream(std::move(ready_stream));
            itr = media_streams.erase(itr);
        } else {
            ++itr;
        }
    }
}

//----------------------------------------------------------------------

void make_clean_sip_headers(uint64_t now, std::future<bool>& write_ready_future)
{
    auto write_ready = write_ready_future.get();


    for (auto itr = sip_headers.begin(); itr != sip_headers.end();) {
        auto& hdr = itr->second;

        if (hdr->ready(now)) {
            if (write_ready) {
                //save_sip_header(hdr.get());

                auto header = std::move(itr->second);
                async_save_header(std::move(header));
            }

            //cache_header(std::move(hdr));
            itr = sip_headers.erase(itr);
        } else {
            ++itr;
        }
    }
}

//----------------------------------------------------------------------

std::unique_ptr<SIPHeader> make_header(uint64_t now, const char* call_id)
{
    //std::cout << __func__ << " " << call_id << std::endl;

    if (!sip_headers_cached.empty()) {
        auto hdr = std::move(sip_headers_cached.front());
        sip_headers_cached.pop_front();

        std::cout << "\r[i] calls " << sip_headers.size() << "; cache " << sip_headers_cached.size() << "      " << std::flush;

        hdr->reset(call_id);
        return std::move(hdr);
    }

    auto hdr = std::make_unique<SIPHeader>(now, call_id);
    return std::move(hdr);
}

//----------------------------------------------------------------------

std::unique_ptr<RTPMediaStream> make_stream(uint64_t now, const NetworkPacket& packet)
{
    auto meta = packet.rtp_meta();
    std::unique_ptr<RTPMediaStream> stream_uptr = nullptr;

    if (!media_streams_cached.empty()) {
        stream_uptr = std::move(media_streams_cached.front());
        media_streams_cached.pop_front();
    } else {
        stream_uptr = std::make_unique<RTPMediaStream>();
    }

    stream_uptr->set_id(meta->id());
    stream_uptr->set_src(packet.src());
    stream_uptr->set_dst(packet.dst());
    stream_uptr->set_sport(packet.sport());
    stream_uptr->set_dport(packet.dport());
    stream_uptr->set_codec(meta->codec());
    stream_uptr->flush(now);


    auto& details = codec_lookup_table.at(meta->codec());
    stream_uptr->set_expected_frame_cnt(CONFIG_CAPTURE_DURATION * details.packet_per_second);

    return std::move(stream_uptr);
}

//----------------------------------------------------------------------

void cache_header(std::unique_ptr<SIPHeader>&& hdr)
{
    if (sip_headers_cached.size() < 20480) {
        hdr->flush();
        sip_headers_cached.emplace_back(std::move(hdr));

        //std::cout << "cached" << std::endl;
    }
}

//----------------------------------------------------------------------

void cache_stream(std::unique_ptr<RTPMediaStream>&& stream)
{
    //std::cout << __func__ << std::endl;


    if (media_streams_cached.size() < 2048) {
        //stream->flush();
        //std::cout << "cached stream " << stream->id() << std::endl;

        media_streams_cached.emplace_back(std::move(stream));
    }
}

//----------------------------------------------------------------------

bool is_write_ready(const std::string& destination_name, uint32_t destination_max_size, uint32_t destination_max_file_cnt)
{
    uint64_t current_size = 0;
    uint32_t current_file_cnt = 0;
    uint64_t max_size = destination_max_size * 1024 * 1024; // megabytes to bytes

    struct stat s;
    auto d = opendir(destination_name.c_str());
    struct dirent* entry = nullptr;

    if (d) {
        while ((entry = readdir(d))) {
            if (++current_file_cnt > destination_max_file_cnt) {
                closedir(d);
                return false;
            }

            std::string path = destination_name + "/" + entry->d_name;

            if (stat(path.c_str(), &s) == -1) {
                continue;
            }

            current_size += s.st_size;

            if (current_size > max_size) {
                closedir(d);
                return false;
            }
        }

        closedir(d);
    } else {
        std::cerr << "[!] Unable to open output directory '" << destination_name << "'." << std::endl;
        return false;
    }

    return true;
}

//----------------------------------------------------------------------

std::future<bool> is_write_ready_rtp()
{
    auto mode = std::launch::async;
    if (!CONFIG_MODE_DUMP_RTP_STREAMS) {
        mode = std::launch::deferred;
    }

    auto write_ready_rtp = std::async(mode, []() {
        return is_write_ready(CONFIG_RTP_DESTINATION_DIRECTORY,
            CONFIG_RTP_DESTINATION_DIRECTORY_MAXSIZE,
            CONFIG_RTP_DESTINATION_DIRECTORY_MAX_FILE_CNT);
    });

    return std::move(write_ready_rtp);
}

//----------------------------------------------------------------------

std::future<bool> is_write_ready_sip()
{
    auto mode = std::launch::async;
    if (!CONFIG_MODE_DUMP_SIP_HEADERS) {
        mode = std::launch::deferred;
    }


    auto write_ready_sip = std::async(mode, []() {
        return is_write_ready(CONFIG_SIP_DESTINATION_DIRECTORY,
            CONFIG_SIP_DESTINATION_DIRECTORY_MAXSIZE,
            CONFIG_SIP_DESTINATION_DIRECTORY_MAX_FILE_CNT);
    });

    return std::move(write_ready_sip);
}

//----------------------------------------------------------------------

std::unique_ptr<RTPMediaStream> get_media_stream(uint32_t ip, uint16_t port)
{
    for (auto itr = media_streams.begin(); itr != media_streams.end();) {
        auto& stream = itr->second;

        if (((stream->src() == ip) && (stream->sport() == port)) || \
            ((stream->dst() == ip) && (stream->dport() == port))) {
            auto found_stream = std::move(itr->second);
            media_streams.erase(itr);

            return std::move(found_stream);
        } else {
            ++itr;
        }
    }

    //std::cout << "\rstream count " << media_streams.size() << std::flush;

    return nullptr;
}


} // namespace assembler


void process_assembler()
{
    using namespace assembler;

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
