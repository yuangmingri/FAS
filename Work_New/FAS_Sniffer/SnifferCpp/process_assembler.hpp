#ifndef __PROCESS_ASSEMBLER_HPP__
#define __PROCESS_ASSEMBLER_HPP__

#include <future>
#include <memory>

#include "VadDetector.h"
#ifdef __cplusplus
extern "C"
{
#endif

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
								
#ifdef __cplusplus
};
#endif



// forward declaration
class Call;
class NetworkPacket;
class RTPMediaStream;
class SIPHeader;


void process_assembler();


namespace assembler {

typedef struct {
    AVCodec *codec;
    AVCodecContext *c;
    AVPacket avpkt;
    AVFrame *decoded_frame;
    AVCodecID codec_id;
    VadDetector *vad;
    short buf[256];
    int bufsamples;
    int total_frames;
    int voice_frames;
} decode_context;

void set_decode_context(decode_context *ctx,AVCodecID codec_id);
void decode_audio(decode_context *ctx);

void acquire_segment();

//----------------------------------------------------------------------

void release_segment();

//----------------------------------------------------------------------

void process_segment(decode_context *);

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

void save_call(const std::string&, Call*,decode_context *);

//----------------------------------------------------------------------

void save_media_stream(const std::string&, const RTPMediaStream*);

//----------------------------------------------------------------------

void async_save_header(std::unique_ptr<SIPHeader>);

//----------------------------------------------------------------------

void async_save_media_stream(const std::string&, std::unique_ptr<RTPMediaStream>,decode_context *);

//----------------------------------------------------------------------

void save_sip_header(SIPHeader*);

//----------------------------------------------------------------------

void make_clean(uint64_t, std::future<bool>&, std::future<bool>&,decode_context *);

//----------------------------------------------------------------------

void make_clean_rtp_streams(uint64_t, std::future<bool>&,decode_context *);

//----------------------------------------------------------------------

void make_clean_sip_headers(uint64_t, std::future<bool>&);

//----------------------------------------------------------------------

std::unique_ptr<SIPHeader> make_header(uint64_t, const char*);

//----------------------------------------------------------------------

std::unique_ptr<RTPMediaStream> make_stream(uint64_t);

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

//----------------------------------------------------------------------

std::unique_ptr<RTPMediaStream> get_media_stream_by_src(uint32_t, uint16_t);


} // namespace assembler


#endif // __PROCESS_ASSEMBLER_HPP__
