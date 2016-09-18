#ifndef __SIP_HEADER_PROCESSING_HPP__
#define __SIP_HEADER_PROCESSING_HPP__

#include <cstdint>
#include <array>
#include <iostream>
#include <memory>
#include <pcre.h>
#include "include.hpp"
#include "sip_metadata.hpp"



constexpr uint16_t OVECTOR_SIZE = 30;


inline
std::string get_single_regex_result(const std::string& line, const std::array<int32_t, OVECTOR_SIZE>& ovector)
{
    uint16_t i = 1; // i = 1 to skip original string
    uint16_t len = ovector[2 * i + 1] - ovector[2 * i];
    uint16_t start = ovector[2 * i];

    return line.substr(start, len);
}

//----------------------------------------------------------------------

void init_regex_patterns();

//----------------------------------------------------------------------

void get_sip_headers(SIPMetadata*, const std::string&);


#endif // __SIP_HEADER_PROCESSING_HPP__
