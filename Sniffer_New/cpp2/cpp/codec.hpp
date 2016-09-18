#ifndef __CODEC_HPP__
#define __CODEC_HPP__

#include <cstdint>
#include <limits>
#include <string>



enum class Codec: uint8_t
{
    g711U     = 0,
    g723      = 4,
    g711A     = 8,
    g729      = 18,
    UNDEFINED = std::numeric_limits<uint8_t>::max()
};

//----------------------------------------------------------------------

struct codec_details
{
    std::string name;
    uint16_t packet_per_second;
};


#endif // __CODEC_HPP__
