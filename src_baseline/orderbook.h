#pragma once
#include "order.h"
#include<vector>

namespace baseline{
    class OrderBook{
        private:
            std::vector<Order> bids;
            std::vector<Order> asks;

            static bool compareBids(const Order& a, const Order& b);
            static bool compareAsks(const Order& a, const Order& b);

            void match_buy(Order& buy_order);
            void match_sell(Order& sell_order);
        public:
            void add_order(Order order);
            void cancel_order(uint64_t target_order_id);
            void print_book() const;
    };
}