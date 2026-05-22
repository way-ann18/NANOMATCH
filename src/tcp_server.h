#pragma once
#include "orderbook.h"
#include <cstdint>
#include <string>

class TCPServer {
private:
    int server_fd;
    int port;
    OrderBook& book;

    // The exact 18-byte structure we expect over the network
    #pragma pack(push, 1) // Force compiler to NOT add padding bytes
    struct BinaryOrderPacket {
        uint64_t order_id;
        uint32_t price;
        uint32_t quantity;
        uint8_t side;
        uint8_t type;
    };
    #pragma pack(pop)

public:
    TCPServer(int listen_port, OrderBook& order_book);
    ~TCPServer();

    void start_listening();
};