#include "process_worker.hpp"
#include <thread>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <boost/interprocess/sync/interprocess_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include "include.hpp"
#include "rtp_media_stream_processing.hpp"
#include "sip_header_processing.hpp"


using boost::interprocess::interprocess_mutex;
using boost::interprocess::scoped_lock;
using boost::interprocess::try_to_lock;


extern bool CONFIG_MODE_DUMP_RTP_STREAMS;
extern bool CONFIG_MODE_DUMP_SIP_HEADERS;


namespace worker {



uint32_t current_packet_cnt = 0;
uint16_t current_segment_cnt = 0;
struct shared_memory_segment* current_segment;
static std::deque<struct shared_memory_segment*> segments;
scoped_lock<interprocess_mutex> segment_lock;

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
            // looking only for captured
            if (!segment->network_packets_flushed && \
                segment->network_packets_captured && \
                !segment->network_packets_processed && \
                !segment->network_packets_assembled) {
                //std::cout << __func__ << " worker shm" << current_segment_cnt << std::endl;

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
    current_segment->network_packets_captured = false;
    current_segment->network_packets_processed = true;

    segment_lock.unlock();

    current_segment = nullptr;
    ++current_segment_cnt;
}

//----------------------------------------------------------------------

void process_segment(decode_context *ctx)
{
    for (uint32_t i = 0; i < current_segment->data.size(); ++i) {
        auto& packet = current_segment->data.at(i);

        analyze(packet,ctx);
    }
}

//----------------------------------------------------------------------

void analyze(NetworkPacket& packet, decode_context *ctx)
{
    uint8_t* ip_packet_ptr = packet.data() + ETHERNET_HEADER_LEN;
    uint16_t ip_packet_len = packet.size() - ETHERNET_HEADER_LEN;

    struct iphdr* ip_hdr = reinterpret_cast<struct iphdr*>(ip_packet_ptr);

    if (ip_hdr->version != 4) {
        packet.set_proto_unknown();
        return;
    }

    if (ip_hdr->protocol != IPPROTO_UDP) {
        packet.set_proto_unknown();
        return;
    }


    static constexpr uint16_t ip_hdr_len = sizeof(struct iphdr);
    static constexpr uint16_t udp_hdr_len = sizeof(struct udphdr);


    uint16_t payload_offset = ip_hdr_len + udp_hdr_len;
    if ((ip_packet_len - payload_offset) <= 0) {
        packet.set_proto_unknown();
        return;
    }

    struct udphdr* udp = (reinterpret_cast<struct udphdr*>(ip_packet_ptr + (ip_hdr->ihl << 2)));

    packet.set_src(ntohl(ip_hdr->saddr));
    packet.set_dst(ntohl(ip_hdr->daddr));
    packet.set_sport(ntohs(udp->source));
    packet.set_dport(ntohs(udp->dest));


    uint8_t* payload_ptr = ip_packet_ptr + payload_offset;
    uint16_t payload_len = ip_packet_len - payload_offset;


    if (packet.sport() == 5060 || packet.dport() == 5060) {
        analyze_sip_header(packet, payload_ptr, payload_len);
    } else {
        if (CONFIG_MODE_DUMP_RTP_STREAMS) {
            analyze_rtp_header(packet, payload_ptr, payload_len, ctx);
        }
    }
}

//----------------------------------------------------------------------

void analyze_sip_header(NetworkPacket& packet, const uint8_t* payload_ptr, uint16_t payload_len)
{
    std::cout << "analyze sip header"  << std::endl;

    std::string payload_str(reinterpret_cast<const char*>(payload_ptr), payload_len);
    get_sip_headers(packet.sip_meta(), payload_str);


    if (std::strlen(packet.sip_meta()->call_id())) {
        packet.set_proto_sip();
        
        std::cout << "call id        " << packet.sip_meta()->call_id() << std::endl;
        std::cout << "request method " << static_cast<uint16_t>(packet.sip_meta()->request_method()) << std::endl;
        std::cout << "response code  " << packet.sip_meta()->response_code() << std::endl;
        std::cout << "address        " << packet.sip_meta()->address() << std::endl;
        std::cout << "audio_port     " << packet.sip_meta()->audio_port() << std::endl;
        std::cout << "from           " << packet.sip_meta()->from() << std::endl;
        std::cout << "to             " << packet.sip_meta()->to() << std::endl;
        std::cout << std::endl;

        std::cout << payload_str << std::endl;

        std::cout << std::endl;
        
        
        return;
    } else {
        std::cout << "call id        " << packet.sip_meta()->call_id() << std::endl;
        std::cout << "request method " << static_cast<uint16_t>(packet.sip_meta()->request_method()) << std::endl;
        std::cout << "response code  " << packet.sip_meta()->response_code() << std::endl;
        std::cout << "address        " << packet.sip_meta()->address() << std::endl;
        std::cout << "audio_port     " << packet.sip_meta()->audio_port() << std::endl;
        std::cout << "from           " << packet.sip_meta()->from() << std::endl;
        std::cout << "to             " << packet.sip_meta()->to() << std::endl;
        std::cout << std::endl;

        std::cout << payload_str << std::endl;

        std::cout << std::endl;
    }

    packet.set_proto_unknown();
}


void decode_audio(decode_context *ctx)
{
    int len = 0;
    static int decimator = 0;

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
            if(decimator % 10 == 0)
            {
                short *paudio = (short*)ctx->decoded_frame->data[ch];
                for(i = 0; i < isamples; i++)
                {
                    printf("%d,", *paudio);
                    paudio++;
                }
                printf("\n");
            }
            decimator += 1;
            fprintf(stderr, "decoded samples=%d, total=%d, buf_samples = %d\n",ctx->decoded_frame->nb_samples,total_samples,ctx->bufsamples);
            while(isamples > 0 )
            {
            	left_samples = total_samples - ctx->bufsamples;
            	if(left_samples <= isamples)
            	{
            		memcpy(&ctx->buf[ctx->bufsamples],ctx->decoded_frame->data[ch] + ((ctx->decoded_frame->nb_samples - isamples)*2), left_samples);
            		bool vad = ctx->vad->process((char*)ctx->buf);
                        if(vad)
                        {
                            printf("voice\n");
                            fprintf(ctx->fp,"voice\n");
                        }else{
                            printf("non voice\n");
                            fprintf(ctx->fp,"non voice\n");
                        }
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
//----------------------------------------------------------------------

void analyze_rtp_header(NetworkPacket& packet, const uint8_t* payload_ptr, uint16_t payload_len, decode_context *ctx)
{
    auto hdr = reinterpret_cast<const struct rtp_header*>(payload_ptr);
    
    if (is_protocol_RTP(hdr, payload_len) && is_rtp_payload_type_allowed(hdr)) {
        auto hdr_len = get_rtp_header_len(hdr);

        const uint8_t* media_payload_ptr = payload_ptr + hdr_len;
        uint16_t media_payload_len = payload_len - hdr_len;
        uint8_t payload_type;
        AVCodecID codec_type;
        
        printf("rtpsize=%d\n",media_payload_len);
        if(ctx->fp == NULL)
        {
            char filename[64];
            time_t tval = time(NULL);
            struct tm *t = localtime(&tval);
            sprintf(filename,"%d_%d_%d_%d_%d_%d.txt",t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
            ctx->fp = fopen(filename,"w"); 
        }
        fprintf(ctx->fp, "rtp payloadsize=%d\n",media_payload_len);
        auto meta = packet.rtp_meta();
        
        payload_type = get_rtp_payload_type(hdr);
        switch(payload_type)
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
        
        if(codec_type != ctx->codec_id)
            set_decode_context(ctx,codec_type);

        meta->set_id(ntohl(hdr->sync_identifier));
        meta->set_sport(packet.sport());
        meta->set_dport(packet.dport());
        meta->set_sequence_number(ntohs(hdr->sequence_number));
        meta->set_codec(static_cast<Codec>(payload_type));
        meta->set_payload_ptr(media_payload_ptr);
        meta->set_payload_len(media_payload_len);

        packet.set_proto_rtp();

        ctx->avpkt.data = (uint8_t *)media_payload_ptr;
        ctx->avpkt.size = media_payload_len;

        decode_audio(ctx);
        return;
    }
    packet.set_proto_unknown();
}


} // namespace worker


void process_worker()
{
    using namespace worker;
    decode_context ctx;
    
    memset(&ctx,0,sizeof(ctx));
    std::cout << __func__ << " " << getpid() << std::endl;

    // wait for all processes to be started
    std::this_thread::sleep_for(std::chrono::seconds(1));

    segments = get_shared_memory_segments();
    if (segments.empty()) {
        exit_nicely();
    }
    
    ctx.vad = new VadDetector(240,8000);
    av_register_all();
    avformat_network_init();

    while (true) {
        acquire_segment();
        process_segment(&ctx);
        release_segment();
    }
}

