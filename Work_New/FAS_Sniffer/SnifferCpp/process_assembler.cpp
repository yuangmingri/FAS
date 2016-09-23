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

#include <libpq-fe.h>

#define ENABLE_FAS

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
std::unordered_map<uint64_t, std::unique_ptr<RTPMediaStream>> media_streams;
std::deque<std::unique_ptr<RTPMediaStream>> media_streams_cached;
std::unordered_map<std::string, std::unique_ptr<SIPHeader>> sip_headers;
std::deque<std::unique_ptr<SIPHeader>> sip_headers_cached;
std::deque<std::future<std::unique_ptr<SIPHeader>>> async_header_io;
std::deque<std::future<std::unique_ptr<RTPMediaStream>>> async_media_stream_io;


void set_decode_context(decode_context *ctx,AVCodecID codec_id)
{
    if(ctx->c)
    {
        avcodec_free_context(&ctx->c);
        av_free(ctx->c);
    }
    if(ctx->decoded_frame)
        av_frame_free(&ctx->decoded_frame);

    av_init_packet(&ctx->avpkt);
    ctx->codec = avcodec_find_decoder(codec_id);
    if (!ctx->codec) {
        fprintf(stderr,"Codec not found\n");
        return;
    }

    ctx->c = avcodec_alloc_context3(ctx->codec);
    if (!ctx->c) {
        fprintf(stderr, "Could not allocate audio codec context\n");
        return;
    }

    ctx->c->channels = 1;
    ctx->c->sample_fmt = AV_SAMPLE_FMT_S16;
    /* open it */
    if (avcodec_open2(ctx->c, ctx->codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return;
    }
    av_frame_free(&ctx->decoded_frame);
    ctx->decoded_frame = NULL;
}


void decode_audio(decode_context *ctx)
{
    int len = 0;

    while (ctx->avpkt.size > 0) {
        int i, ch = 0;
        int got_frame = 0;

        if (!ctx->decoded_frame) {
            if (!(ctx->decoded_frame = av_frame_alloc())) 
            {
                fprintf(stderr, "Could not allocate audio frame\n");
                break;
            }
        }

        len = avcodec_decode_audio4(ctx->c, ctx->decoded_frame, &got_frame, &ctx->avpkt);
        if (len < 0) {
            fprintf(stderr, "Error while decoding\n");
            break;
        }

        if (got_frame) {
            /* if a frame has been decoded, output it */
            int data_size = av_get_bytes_per_sample(ctx->c->sample_fmt);
            if (data_size < 0) {
                /* This should not occur, checking just for paranoia */
                fprintf(stderr, "Failed to calculate data size\n");
                break;
            }
            int isamples = ctx->decoded_frame->nb_samples;
            int left_samples;
            int total_samples = sizeof(ctx->buf)/2;

            short *paudio = (short*)ctx->decoded_frame->data[ch];
           
            //fprintf(ctx->fp, "decoded samples=%d\n",isamples);
            fprintf(stderr, "decoded samples=%d, total=%d, buf_samples = %d\n",ctx->decoded_frame->nb_samples,total_samples,ctx->bufsamples);
            while(isamples > 0 )
            {
            	left_samples = total_samples - ctx->bufsamples;
            	if(left_samples <= isamples)
            	{
            	    ctx->total_frames++;
            		memcpy(&ctx->buf[ctx->bufsamples],ctx->decoded_frame->data[ch] + ((ctx->decoded_frame->nb_samples - isamples)*2), left_samples);
            		bool vad = ctx->vad->process((char*)ctx->buf);
                    if(vad)
                        ctx->voice_frames++;
            		ctx->bufsamples = 0;
                    isamples -= left_samples;
            	}else {
            		memcpy(&ctx->buf[ctx->bufsamples],ctx->decoded_frame->data[ch] + ((ctx->decoded_frame->nb_samples - isamples)*2), isamples);
            		ctx->bufsamples += isamples;
            		isamples = 0;
            	}
            }
        }
        ctx->avpkt.size -= len;
        ctx->avpkt.data += len;
        ctx->avpkt.dts =
        ctx->avpkt.pts = AV_NOPTS_VALUE;
    }
}

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
            if (!segment->network_packets_flushed && !segment->network_packets_captured && segment->network_packets_processed && !segment->network_packets_assembled) {
                std::cout << __func__ << " assembler shm" << current_segment_cnt << std::endl;

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

void process_segment(decode_context *ctx)
{

//    std::cout << "segmet in assemble#" << current_segment_cnt <<  std::endl;

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

    make_clean(now, write_ready_sip, write_ready_rtp,ctx);
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
                auto ringtone_stream = get_media_stream_by_src(ringtone_ip, ringtone_port);
                if (ringtone_stream) {
                    if ((ringtone_ip == call->callee_ip()) && (ringtone_port == call->callee_port())) {
                        //std::cout << "[!] cloning streams" << std::endl;


                        auto callee_stream = make_stream(now);
                        ringtone_stream->move_unexpected_frames(callee_stream);


                        call->set_ringtone_stream(std::move(ringtone_stream));
                        media_streams[callee_stream->unique_id()] = std::move(callee_stream);
                    } else {
                        call->set_ringtone_stream(std::move(ringtone_stream));
                    }
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

void save_call(const std::string& call_id, Call* call,decode_context *ctx)
{
    auto callee_ip = call->callee_ip();
    auto caller_ip = call->caller_ip();
    auto ringtone_ip = call->ringtone_ip();
    auto callee_port = call->callee_port();
    auto caller_port = call->caller_port();
    auto ringtone_port = call->ringtone_port();


    if (caller_ip && caller_port) {
        auto caller_stream = get_media_stream_by_src(caller_ip, caller_port);
        if (caller_stream) {
            std::string filename = CONFIG_RTP_DESTINATION_DIRECTORY + "/" + call_id + "_caller.media";
            async_save_media_stream(call_id,filename, std::move(caller_stream),ctx);
        } else {
            //std::cout << "caller stream not found " << get_ip_str(caller_ip) << ":" << caller_port << std::endl;
        }
    }

    if (callee_ip && callee_port) {
        auto callee_stream = get_media_stream_by_src(callee_ip, callee_port);
        if (callee_stream) {
            std::string filename = CONFIG_RTP_DESTINATION_DIRECTORY + "/" + call_id + "_callee.media";
            async_save_media_stream(call_id,filename, std::move(callee_stream),ctx);
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
            async_save_media_stream(call_id,filename, std::move(ringtone_stream),ctx);
        } else {
            //std::cout << "ringtone stream not found " << get_ip_str(ringtone_ip) << ":" << ringtone_port << std::endl;
        }
    }
}

//----------------------------------------------------------------------

PGconn* postgress_connect()
{
	PGconn *conn = NULL;
	char conninfo_str[256];
    char sSql[256];
    
	sprintf(conninfo_str, "user='postgres' password='' dbname='class4_v5' hostaddr='127.0.0.1'");
	conn = PQconnectdb(conninfo_str);

	// Check to see that the back-end connection was successfully made
	if (PQstatus(conn) != CONNECTION_OK)
	{
		printf("Connection to database failed\n");
		PQfinish(conn);
		return NULL;
	}
    

	PGresult *res = NULL;
	sprintf(sSql, "CREATE TABLE IF NOT EXISTS public.fas_check_result (ID serial PRIMARY KEY, callid VARCHAR (255) NOT NULL, calltime VARCHAR (255) NOT NULL, result smallint);");
	res = PQexec(conn, sSql);

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("Create table failed\n");
		PQclear(res);
		PQfinish(conn);
		return NULL;
	}	
	// Clear result
	PQclear(res);
	return conn;
}
int InsertRecord(PGconn *conn, const char* table_name, const char* calltime, const char *callid,int result)
{
	// Append the SQL statement
	char sSql[256];
	sprintf(sSql, "INSERT INTO %s(callid,calltime,result) VALUES('%s', '%s', '%d')", table_name, calltime, callid, result);

	// Execute with SQL statement
	PGresult *res = PQexec(conn, sSql);

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		printf("Insert record failed\n");
		PQclear(res);
		PQfinish(conn);
		return -1;
	}

	// Clear result
	PQclear(res);
	return 0;
}
void save_vad_result(const std::string& callid,const std::string& filename, const RTPMediaStream* stream,decode_context *ctx)
{
    int codec = static_cast<int>(stream->codec());
    char buf[128];
    FILE *fp;
    AVCodecID codec_type;
    
    sprintf(buf,"%s.txt",filename.c_str());    
    printf("save_vad_result filename=%s\n",buf);
    fp = fopen(buf,"w");
    switch(codec)
    {
        case (int)Codec::g711U:
            codec_type = AV_CODEC_ID_PCM_MULAW;
            break;
        case (int)Codec::g723:
            codec_type = AV_CODEC_ID_G723_1;
            break;
        case (int)Codec::g711A:
            codec_type = AV_CODEC_ID_PCM_ALAW;
            break;
        case (int)Codec::g729:
            codec_type = AV_CODEC_ID_G729;
            break;
        default:
            codec_type = AV_CODEC_ID_NONE;
    }
    set_decode_context(ctx,codec_type);
    ctx->total_frames = 0;
    ctx->voice_frames = 0;
    auto frame_cnt = stream->frame_cnt();
    for (int i = 0; i < frame_cnt; ++i) {
        auto& frame = stream->frames().at(i);        
        ctx->avpkt.data = (uint8_t *)frame->data();
        ctx->avpkt.size = frame->data_len();
        decode_audio(ctx);
    }
      
    fprintf(fp,"total_frames=%d\n",ctx->total_frames);
    fprintf(fp,"voice_frames=%d\n",ctx->voice_frames);
    fclose(fp);
    
    // Saving to postgres database
    if(callid.find("callee") != std::string::npos)
    {
        PGconn *conn = NULL;
	    conn = postgress_connect();
	    time_t tval = time(NULL);
        struct tm *t = localtime(&tval);
        char timebuf[64];
        sprintf(timebuf,"%d.%02d.%02d %02d:%02d:%02d.txt",t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
        int result = 1;
        if(ctx->voice_frames > 5)
            result = 1;
        else
            result = 0;
            
        if(conn)
        {
            InsertRecord(conn,"public.fas_check_result",timebuf,callid.c_str(),result);
            PQfinish(conn);
        }
    }
    
    
}

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
            auto expected_frame_cnt = stream->expected_frame_cnt();
            if (frame_cnt > expected_frame_cnt) {
                frame_cnt = expected_frame_cnt;
            }

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

void async_save_media_stream(const std::string& callid,const std::string& filename, std::unique_ptr<RTPMediaStream> stream_uptr,decode_context *ctx)
{
    auto stream = stream_uptr.get();
    stream_uptr.release();

    save_vad_result(callid,filename,stream,ctx);
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
            h.ts.tv_sec = packet->ts().tv_sec;
            h.ts.tv_usec = packet->ts().tv_usec;

            pcap_dump(reinterpret_cast<u_char*>(dumper), &h, packet->data());
        }

        pcap_dump_close(dumper);
    } else {
        std::cerr << "[!] unable to save .pcap (destination directory doesn't exists)" << std::endl;
    }
}

//----------------------------------------------------------------------

void make_clean(uint64_t now, std::future<bool>& write_ready_future_sip, std::future<bool>& write_ready_future_rtp,decode_context *ctx)
{
    if (CONFIG_MODE_DUMP_SIP_HEADERS) {
        make_clean_sip_headers(now, write_ready_future_sip);
    }
    if (CONFIG_MODE_DUMP_RTP_STREAMS) {
        make_clean_rtp_streams(now, write_ready_future_rtp,ctx);
    }
}

//----------------------------------------------------------------------

void make_clean_rtp_streams(uint64_t now, std::future<bool>& write_ready_future,decode_context *ctx)
{
    auto write_ready = write_ready_future.get();


    for (auto itr = calls.begin(); itr != calls.end();) {
        auto& call = itr->second;

        if (call->ready(now)) {
            if (write_ready) {
                save_call(itr->first, call.get(),ctx);
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

std::unique_ptr<RTPMediaStream> make_stream(uint64_t now)
{
    std::unique_ptr<RTPMediaStream> stream_uptr = nullptr;

    if (!media_streams_cached.empty()) {
        stream_uptr = std::move(media_streams_cached.front());
        media_streams_cached.pop_front();
    } else {
        stream_uptr = std::make_unique<RTPMediaStream>();
    }

    stream_uptr->flush(now);


    return stream_uptr;
}

//----------------------------------------------------------------------

std::unique_ptr<RTPMediaStream> make_stream(uint64_t now, const NetworkPacket& packet)
{
    auto meta = packet.rtp_meta();
    auto stream_uptr = make_stream(now);

    stream_uptr->set_id(meta->id());
    stream_uptr->set_src(packet.src());
    stream_uptr->set_dst(packet.dst());
    stream_uptr->set_sport(packet.sport());
    stream_uptr->set_dport(packet.dport());
    stream_uptr->set_codec(meta->codec());


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

//----------------------------------------------------------------------

std::unique_ptr<RTPMediaStream> get_media_stream_by_src(uint32_t ip, uint16_t port)
{
    for (auto itr = media_streams.begin(); itr != media_streams.end();) {
        auto& stream = itr->second;

        if ((stream->src() == ip) && (stream->sport() == port)) {
            auto found_stream = std::move(itr->second);
            media_streams.erase(itr);

            return std::move(found_stream);
        } else {
            ++itr;
        }
    }

    return nullptr;
}


} // namespace assembler


void process_assembler()
{
    using namespace assembler;
	decode_context ctx;
	
    std::cout << __func__ << " " << getpid() << std::endl;

    // wait for all processes to be started
    std::this_thread::sleep_for(std::chrono::seconds(1));

    segments = get_shared_memory_segments();
    if (segments.empty()) {
        exit_nicely();
    }
    
    memset(&ctx,0,sizeof(ctx));
#ifdef ENABLE_FAS    
    ctx.vad = new VadDetector(256,8000);
    av_register_all();
    avformat_network_init();
#endif

    while (true) {
        acquire_segment();
        process_segment(&ctx);
        release_segment();
    }
}
