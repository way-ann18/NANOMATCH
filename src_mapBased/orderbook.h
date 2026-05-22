#pragma once

#include "order.h"
#include <map>
#include <queue>
#include <cstdint>

namespace map_queue {

    struct PriceLevel {
        uint64_t price;
        uint64_t total_volume = 0;
        std::queue<Order> orders;

        void add_order(const Order& order) {
            orders.push(order);
            total_volume += order.quantity;
        }
    };

    class MapOrderBook {
    private:
        std::map<uint64_t, PriceLevel, std::greater<uint64_t>> bids;
        std::map<uint64_t, PriceLevel> asks;

        void match();

    public:
        MapOrderBook() = default;

        void add_order(const Order& order);
        void cancel_order(uint64_t order_id);
        void print_top_of_book() const;
    };

}