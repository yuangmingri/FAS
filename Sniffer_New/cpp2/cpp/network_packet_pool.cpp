#include "network_packet_pool.hpp"
#include <iostream>



NetworkPacketPool::NetworkPacketPool(uint32_t size)
{
    pool_size_ = size;

    //for (uint32_t i = 0; i < pool_size_; ++i) {
    //    pool_.push_back(std::make_unique<NetworkPacket>());
    //}
}

//----------------------------------------------------------------------

std::unique_ptr<NetworkPacket> NetworkPacketPool::get_packet()
{
    return nullptr;
}






/*
std::unique_ptr<NetworkPacket> NetworkPacketPool::get_packet()
{

    static uint32_t packet_id = 0;

    if (packet_id == pool_size_) {
        packet_id = 0; // set in initial state
    }

    uint16_t attempt = 10;
    while (--attempt) {
        auto& packet = pool_.at(packet_id);
        ++packet_id;

        if (packet.unique()) {
            return packet;
        }
    }

    try {
        //std::cout << __func__ << std::endl;
        return std::make_unique<NetworkPacket>();
    } catch (...) {
        return nullptr;
    }


    for (const auto& packet: pool_) {
        if (packet.use_count() == 1) {
            return packet;
        }
    }

    // to avoid std::bad_alloc
    try {
        auto packet = std::make_shared<NetworkPacket>();
        if (pool_.size() < pool_size_) {
            pool_.push_back(packet);
        }
        else {
            std::cout << "[inf] " << __func__ << " created new packet. Pool size " << pool_.size() << std::endl;
        }

        return packet;
    } catch (...) {
    }

    return nullptr;
}
*/
