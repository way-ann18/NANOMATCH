#pragma once
#include "ring_buffer.h"
#include <thread>
#include <string>
#include <fstream>
#include <cstdint>

// The exact data we want to log when a trade occurs
struct TradeLog {
    uint64_t timestamp;
    uint64_t buyer_order_id;
    uint64_t seller_order_id;
    uint32_t price;
    uint32_t quantity;
};

class Logger {
private:
    // Instantiate the Lock-Free Ring Buffer to hold 1,048,576 logs
    SPSCRingBuffer<TradeLog, 1048576> ring_buffer; 

    std::thread background_thread;
    std::atomic<bool> running;
    std::ofstream log_file;

    void process_logs();

public:
    Logger(const std::string& filepath);
    ~Logger();

    // Called by the Main Thread inside execute_limit_buy()
    void log_trade(uint64_t buyer_id, uint64_t seller_id, uint32_t price, uint32_t qty);
};