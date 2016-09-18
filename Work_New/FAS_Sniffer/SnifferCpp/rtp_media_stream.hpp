#ifndef __RTP_MEDIA_STREAM_HPP__
#define __RTP_MEDIA_STREAM_HPP__

#include <cstdint>
#include <chrono>
#include <string>
#include <vector>
#include "include.hpp"
#include "media_frame.hpp"



class RTPMediaStream
{
    public:
        void add_frame(uint64_t, uint16_t, const uint8_t*, uint16_t);
        void flush(uint64_t);
        uint32_t id() const;
        uint32_t src() const;
        uint32_t dst() const;
        uint16_t sport() const;
        uint16_t dport() const;
        Codec codec() const;
        uint32_t frame_cnt() const;
        uint32_t expected_frame_cnt() const;
        const std::vector<std::unique_ptr<MediaFrame>>& frames() const;
        void set_id(uint32_t);
        void set_src(uint32_t);
        void set_dst(uint32_t);
        void set_sport(uint16_t);
        void set_dport(uint16_t);
        void set_codec(Codec);
        void set_expected_frame_cnt(uint32_t);
        bool ready(uint64_t);
    private:
        void update_last_received(uint64_t);

        uint32_t id_;
        uint32_t src_;
        uint32_t dst_;
        uint16_t sport_;
        uint16_t dport_;
        Codec codec_;
        uint32_t expected_frame_cnt_;
        uint32_t received_frame_cnt_;
        std::vector<std::unique_ptr<MediaFrame>> frames_;
        uint64_t last_received_;
};


#endif // __RTP_MEDIA_STREAM_HPP__
