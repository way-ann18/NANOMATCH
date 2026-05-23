#pragma once
#include "orderbook.h"
#include <cstdint>
#include <string>
#include "ring_buffer.h"

class TCPServer {
private:
    int server_fd;
    int port;
    SPSCRingBuffer<Order, 131072>& buffer;

    #pragma pack(push, 1)
    struct BinaryOrderPacket {
        uint64_t order_id;
        uint32_t price;
        uint32_t quantity;
        uint8_t side;
        uint8_t type;
    };
    #pragma pack(pop)

public:
    TCPServer(int listen_port, SPSCRingBuffer<Order, 131072>& order_buffer);
    ~TCPServer();

    void start_listening();
};