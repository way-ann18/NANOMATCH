#pragma once
#include<cstdint>

enum class Side:uint8_t{
    BUY=0,
    SELL=1
};

enum class OrderType:uint8_t{
    LIMIT=0,
    MARKET=1
};

struct alignas(64) Order{
    uint64_t order_id;
    uint64_t timestamp;
    uint32_t price;
    uint32_t quantity;
    Side side;
    OrderType type;
    int32_t next_index = -1; 
    int32_t prev_index = -1;
};