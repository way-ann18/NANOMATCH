#pragma once

#include <cstdint>

namespace map_queue {

    struct Order {
        uint64_t order_id;
        bool is_buy;
        uint64_t price;
        uint64_t quantity;
    };

}