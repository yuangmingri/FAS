#include "rtp_media_stream_processing.hpp"
#include <iostream>


/*
void print_rtp_header_details(const struct network_connection& conn,
    struct rtp_header* hdr, uint16_t packet_len)
{
    auto header_len = get_rtp_header_len(hdr);

    std::stringstream out;
    out << "src              - " << get_ip_str(conn.src) << ":" << conn.sport << std::endl;
    out << "dst              - " << get_ip_str(conn.dst) << ":" << conn.dport << std::endl;
    out << "------------------------------------------------------------" << std::endl;
    out << "version          - " << get_rtp_version(hdr) << std::endl;
    out << "padding flag     - " << get_rtp_padding_flag(hdr) << std::endl;
    out << "extension flag   - " << get_rtp_extension_flag(hdr) << std::endl;
    out << "CSRC count       - " << get_rtp_csrc_count(hdr) << std::endl;
    out << "marker flag      - " << get_rtp_market_flag(hdr) << std::endl;
    out << "payload type     - " << get_rtp_payload_type(hdr) << std::endl;
    out << "sequence number  - " << ntohs(hdr->sequence_number) << std::endl;
    out << "timestamp        - " << ntohl(hdr->timestamp) << std::endl;
    out << "sync identifier  - " << ntohl(hdr->sync_identifier) << std::endl;
    out << "------------------------------------------------------------" << std::endl;
    out << "header len       - " << header_len << std::endl;
    out << "payload len      - " << packet_len - header_len << std::endl;
    out << "packet len       - " << packet_len << std::endl;

    std::cout << std::endl << out.str();
}
*/
