#include "sip_header_processing.hpp"



static pcre* re_request_method;
static pcre* re_response_code;
static pcre* re_from;
static pcre* re_to;
static pcre* re_callid;
static pcre* re_address;
static pcre* re_audio_port;


void init_regex_patterns()
{
    std::string re_pattern_request_method = "^([A-Z]+)\\s+.*$";
    std::string re_pattern_response_code = "^SIP/\\d\\.\\d\\s+(\\d{3})\\s+.*$";
    std::string re_pattern_from = "^(?:From|f):.*?<(.*)>.*?$";
    std::string re_pattern_to = "^(?:To|t):.*?<(.*)>.*?$";
    std::string re_pattern_callid = "^(?:Call-ID|i):\\s*(.*)\\s*$";
    std::string re_pattern_address = "^c=.*?([0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}).*?$";
    std::string re_pattern_audio_port = "^m=audio\\s+(\\d+).*?$";

    int32_t erroffset;
    const char* error;
    int32_t options = 0;

    re_request_method = pcre_compile(re_pattern_request_method.c_str(), options, &error, &erroffset, nullptr);
    if (!re_request_method) {
        std::cout << "unable to compile pcre pattern '" << re_pattern_request_method << "'." << std::endl;
        exit(EXIT_FAILURE);
    }
    re_response_code = pcre_compile(re_pattern_response_code.c_str(), options, &error, &erroffset, nullptr);
    if (!re_response_code) {
        std::cout << "unable to compile pcre pattern '" << re_pattern_response_code << "'." << std::endl;
        exit(EXIT_FAILURE);
    }
    re_from = pcre_compile(re_pattern_from.c_str(), options, &error, &erroffset, nullptr);
    if (!re_from) {
        std::cout << "unable to compile pcre pattern '" << re_pattern_from << "'." << std::endl;
        exit(EXIT_FAILURE);
    }
    re_to = pcre_compile(re_pattern_to.c_str(), options, &error, &erroffset, nullptr);
    if (!re_to) {
        std::cout << "unable to compile pcre pattern '" << re_pattern_to << "'." << std::endl;
        exit(EXIT_FAILURE);
    }
    re_callid = pcre_compile(re_pattern_callid.c_str(), options, &error, &erroffset, nullptr);
    if (!re_callid) {
        std::cout << "unable to compile pcre pattern '" << re_pattern_callid << "'." << std::endl;
        exit(EXIT_FAILURE);
    }
    re_address = pcre_compile(re_pattern_address.c_str(), options, &error, &erroffset, nullptr);
    if (!re_address) {
        std::cout << "unable to compile pcre pattern '" << re_pattern_address << "'." << std::endl;
        exit(EXIT_FAILURE);
    }
    re_audio_port = pcre_compile(re_pattern_audio_port.c_str(), options, &error, &erroffset, nullptr);
    if (!re_audio_port) {
        std::cout << "unable to compile pcre pattern '" << re_pattern_audio_port << "'." << std::endl;
        exit(EXIT_FAILURE);
    }
}

//----------------------------------------------------------------------

void get_sip_headers(SIPMetadata* sip_meta, const std::string& header)
{
    int16_t cnt = 0;
    std::array<int32_t, OVECTOR_SIZE> ovector;
    std::string line;
    uint16_t line_cnt = 0;
    std::stringstream input(header);

    while (std::getline(input, line)) {
        ++line_cnt;

        // Method and Responce Code
        if (line_cnt == 1) {
            cnt = pcre_exec(re_request_method, nullptr, line.c_str(), line.length(), 0, PCRE_NEWLINE_ANYCRLF, ovector.data(), ovector.size());
            if (cnt > 1) {
                auto method = get_single_regex_result(line, ovector);
                if (method == "BYE") {
                    sip_meta->set_request_method(SIPMetadata::RequestMethod::BYE);
                } else if (method == "CANCEL") {
                    sip_meta->set_request_method(SIPMetadata::RequestMethod::CANCEL);
                } else if (method == "INVITE") {
                    sip_meta->set_request_method(SIPMetadata::RequestMethod::INVITE);
                }

                continue;
            }


            cnt = pcre_exec(re_response_code, nullptr, line.c_str(), line.length(), 0, PCRE_NEWLINE_ANYCRLF, ovector.data(), ovector.size());
            if (cnt > 1) {
                auto code = get_single_regex_result(line, ovector);
                sip_meta->set_response_code(std::atoi(code.c_str()));
                continue;
            }
        }


        // FROM field
        if (!std::strlen(sip_meta->from())) {
            cnt = pcre_exec(re_from, nullptr, line.c_str(), line.length(), 0, PCRE_NEWLINE_ANYCRLF, ovector.data(), ovector.size());
            if (cnt > 1) {
                sip_meta->set_from(get_single_regex_result(line, ovector));
                continue;
            }
        }


        // TO field
        if (!std::strlen(sip_meta->to())) {
            cnt = pcre_exec(re_to, nullptr, line.c_str(), line.length(), 0, PCRE_NEWLINE_ANYCRLF, ovector.data(), ovector.size());
            if (cnt > 1) {
                sip_meta->set_to(get_single_regex_result(line, ovector));
                continue;
            }
        }


        // Call ID field
        if (!std::strlen(sip_meta->call_id())) {
            cnt = pcre_exec(re_callid, nullptr, line.c_str(), line.length(), 0, PCRE_NEWLINE_ANYCRLF, ovector.data(), ovector.size());
            if (cnt > 1) {
                sip_meta->set_call_id(get_single_regex_result(line, ovector));
                continue;
            }
        }


        // Address field
        if (sip_meta->address() == 0) {
            cnt = pcre_exec(re_address, nullptr, line.c_str(), line.length(), 0, PCRE_NEWLINE_ANYCRLF, ovector.data(), ovector.size());
            if (cnt > 1) {
                auto address = get_single_regex_result(line, ovector);
                sip_meta->set_address(get_ip_from_str(address));
                continue;
            }
        }


        // Audio port field
        if (sip_meta->audio_port() == 0) {
            cnt = pcre_exec(re_audio_port, nullptr, line.c_str(), line.length(), 0, PCRE_NEWLINE_ANYCRLF, ovector.data(), ovector.size());
            if (cnt > 1) {
                auto port = get_single_regex_result(line, ovector);
                sip_meta->set_audio_port(std::atoi(port.c_str()));
                continue;
            }
        }
    }
}
