#ifndef __PROCESS_WORKER_HPP__
#define __PROCESS_WORKER_HPP__

#include "network_packet.hpp"
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

typedef struct {
    AVCodec *codec;
    AVCodecContext *c;
    AVPacket avpkt;
    AVFrame *decoded_frame;
    AVCodecID codec_id;
    VadDetector *vad;
    short buf[256];
    int bufsamples;
    FILE *fp;
    FILE *fp2;
} decode_context;

void set_decode_context(decode_context *ctx,AVCodecID codec_id);
void decode_audio(decode_context *ctx);

void process_worker();

namespace worker {


void acquire_segment();

//----------------------------------------------------------------------

void release_segment();

//----------------------------------------------------------------------

void process_segment(decode_context *ctx);

//----------------------------------------------------------------------

void analyze(NetworkPacket&,decode_context *ctx);

//----------------------------------------------------------------------

void analyze_sip_header(NetworkPacket&, const uint8_t*, uint16_t);

//----------------------------------------------------------------------

void analyze_rtp_header(NetworkPacket&, const uint8_t*, uint16_t, decode_context *);


} // namespace worker


#endif // __PROCESS_WORKER_HPP__
