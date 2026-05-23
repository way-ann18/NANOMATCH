#pragma once
#include "order.h"
#include "memory_pool.h"
#include "logger.h"
#include <vector>
#include <iostream>
#include <cstdint>

struct PriceLevel {
    int32_t head_order_index = -1;  
    int32_t tail_order_index = -1;  
    uint32_t total_volume = 0;      
};

class OrderBook {
    private:
        MemoryPool& pool;
        Logger& logger;
        uint32_t MAX_PRICE;

        std::vector<PriceLevel> bids; 
        std::vector<PriceLevel> asks;
        std::vector<int32_t> order_map; 

        std::vector<uint64_t> bid_bitboard;
        std::vector<uint64_t> ask_bitboard;

        uint32_t best_bid;
        uint32_t best_ask;

        void add_to_list(PriceLevel& level, int32_t order_idx);
        void remove_from_list(PriceLevel& level, int32_t order_idx);

        void execute_limit_buy(int32_t incoming_idx);
        void execute_limit_sell(int32_t incoming_idx);
        void execute_market_buy(int32_t incoming_idx);
        void execute_market_sell(int32_t incoming_idx);

        inline void set_bid_active(uint32_t price) {
            bid_bitboard[price / 64] |= (1ULL << (price % 64));
            if (price > best_bid) best_bid = price; 
        }

        inline void clear_bid_active(uint32_t price) {
            bid_bitboard[price / 64] &= ~(1ULL << (price % 64));
        }

        inline void set_ask_active(uint32_t price) {
            ask_bitboard[price / 64] |= (1ULL << (price % 64));
            if (price < best_ask) best_ask = price;
        }

        inline void clear_ask_active(uint32_t price) {
            ask_bitboard[price / 64] &= ~(1ULL << (price % 64));
        }

        void update_best_bid_downwards();
        void update_best_ask_upwards();

    public:

        OrderBook(MemoryPool& memory_pool, Logger& trade_logger, uint32_t max_prices = 1000000, uint32_t max_orders = 2000000) 
            : pool(memory_pool), logger(trade_logger), MAX_PRICE(max_prices) {
            
            bids.resize(max_prices);
            asks.resize(max_prices);
            order_map.assign(max_orders, -1); 
            
            bid_bitboard.assign((max_prices / 64) + 1, 0);
            ask_bitboard.assign((max_prices / 64) + 1, 0);
            
            best_bid = 0;
            best_ask = MAX_PRICE;
        }

        void add_order(const Order& incoming_order);
        void cancel_order(uint64_t order_id);
        void print_book();
};