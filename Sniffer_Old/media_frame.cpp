#include "media_frame.hpp"
#include <cstring>



uint8_t* MediaFrame::data()
{
    return data_.data();
}

//----------------------------------------------------------------------

const uint8_t* MediaFrame::data() const
{
    return data_.data();
}

//----------------------------------------------------------------------

uint16_t MediaFrame::data_len() const
{
    return data_len_;
}

//----------------------------------------------------------------------

uint16_t MediaFrame::sequence_number() const
{
    return sequence_number_;
}

//----------------------------------------------------------------------

void MediaFrame::set_data(const uint8_t* data_ptr, uint16_t data_len)
{
    if (data_len > MediaFrame::maxlength) {
        return;
    }

    std::memcpy(data_.data(), data_ptr, data_len);
    data_len_ = data_len;
}

//----------------------------------------------------------------------

void MediaFrame::set_sequence_number(uint16_t sequence_number)
{
    sequence_number_ = sequence_number;
}
