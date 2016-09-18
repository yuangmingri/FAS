#include "rtp_media_stream.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
#include "include.hpp"



extern uint16_t CONFIG_CAPTURE_DURATION;


void RTPMediaStream::add_frame(uint64_t now, uint16_t sequence_number, const uint8_t* payload_ptr, uint16_t payload_len)
{
    if (frame_cnt_ >= expected_frame_cnt_ * 2) {
        return;
    }


    if (payload_len <= MediaFrame::maxlength) {
        auto& frame = frames_.at(frame_cnt_);

        frame->set_data(payload_ptr, payload_len);
        frame->set_sequence_number(sequence_number);

        ++frame_cnt_;
        update_last_received(now);
    } else {
        std::cout << "[!] unable to add media frame, wrong payload length " << payload_len << std::endl;
    }
}

//----------------------------------------------------------------------

void RTPMediaStream::flush(uint64_t now)
{
    frame_cnt_ = 0;
    update_last_received(now);


    //std::cout << "<<<< STREAM >>>> " << std::hex << id() << std::dec << " from ";
    //std::cout << get_ip_str(src()) << ":" << sport() << " -> to ";
    //std::cout << get_ip_str(dst()) << ":" << dport() << std::endl;
}

//----------------------------------------------------------------------

uint32_t RTPMediaStream::id() const
{
    return id_;
}

//----------------------------------------------------------------------

uint64_t RTPMediaStream::unique_id() const
{
    uint64_t unique_id = 0;
    uint64_t stream_id = id();
    uint32_t stream_sport = sport();
    uint32_t stream_dport = dport();

    unique_id = unique_id | stream_id << 32;
    unique_id = unique_id | stream_sport << 16;
    unique_id = unique_id | stream_dport;

    return unique_id;
}

//----------------------------------------------------------------------

uint32_t RTPMediaStream::src() const
{
    return src_;
}

//----------------------------------------------------------------------

uint32_t RTPMediaStream::dst() const
{
    return dst_;
}

//----------------------------------------------------------------------

uint16_t RTPMediaStream::sport() const
{
    return sport_;
}

//----------------------------------------------------------------------

uint16_t RTPMediaStream::dport() const
{
    return dport_;
}

//----------------------------------------------------------------------

Codec RTPMediaStream::codec() const
{
    return codec_;
}

//----------------------------------------------------------------------

uint32_t RTPMediaStream::frame_cnt() const
{
    return frame_cnt_;
}

//----------------------------------------------------------------------

uint32_t RTPMediaStream::expected_frame_cnt() const
{
    return expected_frame_cnt_;
}

//----------------------------------------------------------------------

const std::vector<std::unique_ptr<MediaFrame>>& RTPMediaStream::frames() const
{
    return frames_;
}

//----------------------------------------------------------------------

void RTPMediaStream::move_unexpected_frames(const std::unique_ptr<RTPMediaStream>& stream)
{
    stream->set_id(id());
    stream->set_src(src());
    stream->set_dst(dst());
    stream->set_sport(sport());
    stream->set_dport(dport());
    stream->set_codec(codec());
    stream->set_frame_cnt(0);
    stream->set_expected_frame_cnt(expected_frame_cnt());

    auto cnt = frame_cnt();
    auto expected_cnt = expected_frame_cnt();

    if (cnt > expected_cnt) {
        auto to_copy_cnt = cnt - expected_cnt;

        stream->set_frame_cnt(to_copy_cnt);
        for (uint32_t i = 0; i < to_copy_cnt; ++i) {
            auto& src_frame = frames_.at(expected_cnt + i);
            auto& dst_frame = stream->frames().at(i);

            *dst_frame = *src_frame;
        }

        set_frame_cnt(expected_cnt);
    }
}

//----------------------------------------------------------------------

void RTPMediaStream::set_id(uint32_t id)
{
    id_ = id;
}

//----------------------------------------------------------------------

void RTPMediaStream::set_src(uint32_t src)
{
    src_ = src;
}

//----------------------------------------------------------------------

void RTPMediaStream::set_dst(uint32_t dst)
{
    dst_ = dst;
}

//----------------------------------------------------------------------

void RTPMediaStream::set_sport(uint16_t sport)
{
    sport_ = sport;
}

//----------------------------------------------------------------------

void RTPMediaStream::set_dport(uint16_t dport)
{
    dport_ = dport;
}

//----------------------------------------------------------------------

void RTPMediaStream::set_codec(Codec codec)
{
    codec_ = codec;
}

//----------------------------------------------------------------------

void RTPMediaStream::set_frame_cnt(uint32_t frame_cnt)
{
    frame_cnt_ = frame_cnt;
}

//----------------------------------------------------------------------

void RTPMediaStream::set_expected_frame_cnt(uint32_t expected_frame_cnt)
{
    expected_frame_cnt_ = expected_frame_cnt;

    while (frames_.size() < expected_frame_cnt_ * 2) {
        auto frame = std::make_unique<MediaFrame>();
        frames_.emplace_back(std::move(frame));
    }
}

//----------------------------------------------------------------------

bool RTPMediaStream::ready(uint64_t now)
{
    auto seconds = now - last_received_;
    if (seconds >= CONFIG_CAPTURE_DURATION * 7) {
        return true;
    }

    return false;
}

//----------------------------------------------------------------------

void RTPMediaStream::update_last_received(uint64_t now)
{
    last_received_ = now;
}
