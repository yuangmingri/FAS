#ifndef __MEDIA_FRAME_HPP__
#define __MEDIA_FRAME_HPP__

#include <array>



class MediaFrame
{
    public:
        static constexpr uint16_t maxlength = 256; 

        uint8_t* data();
        const uint8_t* data() const;
        uint16_t data_len() const;
        uint16_t sequence_number() const;
        void set_data(const uint8_t*, uint16_t);
        void set_sequence_number(uint16_t);
    private:
        uint16_t data_len_;
        uint16_t sequence_number_;
        std::array<uint8_t, MediaFrame::maxlength> data_;
};


#endif // __MEDIA_FRAME_HPP__
