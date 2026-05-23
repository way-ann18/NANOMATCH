#pragma once

#include "order.h"
#include <map>
#include <list>             // CHANGED: From queue to list
#include <unordered_map>    // NEW: For O(1) lookups
#include <cstdint>

namespace map_queue {

    struct PriceLevel {
        uint64_t price;
        uint64_t total_volume = 0;
        std::list<Order> orders; // CHANGED: Now a doubly-linked list

        // MODIFIED: Returns the iterator to the newly inserted order
        std::list<Order>::iterator add_order(const Order& order) {
            orders.push_back(order);
            total_volume += order.quantity;
            return std::prev(orders.end()); 
        }
    };

    class MapOrderBook {
    private:
        std::map<uint64_t, PriceLevel, std::greater<uint64_t>> bids;
        std::map<uint64_t, PriceLevel> asks;

        // NEW: Global Hash Map for O(1) cancellation
        // Maps order_id -> { Pointer to its PriceLevel, Iterator to the Order in the list }
        std::unordered_map<uint64_t, std::pair<PriceLevel*, std::list<Order>::iterator>> order_lookup;


    public:
        MapOrderBook() = default;

        void add_order(const Order order);
        void cancel_order(uint64_t order_id);
        void print_top_of_book() const;
    };

}