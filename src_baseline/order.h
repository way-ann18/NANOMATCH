#pragma once
#include<cstdint>

namespace baseline{
    enum class Side:uint8_t{
        BUY,
        SELL
    };

    enum class OrderType:uint8_t{
        LIMIT,
        MARKET
    };

    struct alignas(32) Order{
        uint64_t order_id;
        uint64_t timestamp;
        uint32_t price;
        uint32_t quantity;
        Side side;
        OrderType type;
    };
}