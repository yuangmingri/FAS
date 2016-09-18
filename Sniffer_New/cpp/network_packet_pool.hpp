#ifndef __NETWORK_PACKET_POOL_HPP__
#define __NETWORK_PACKET_POOL_HPP__

#include <vector>
#include <memory>
#include <thread>
#include "network_packet.hpp"



class NetworkPacketPool
{
    public:
        NetworkPacketPool(uint32_t);

        std::unique_ptr<NetworkPacket> get_packet();
    private:
        uint32_t pool_size_;
        std::vector<std::unique_ptr<NetworkPacket>> pool_;
};


#endif // __NETWORK_PACKET_POOL_HPP__
