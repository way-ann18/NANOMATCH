#include "logger.h"
#include <iostream>
#include <chrono>

Logger::Logger(const std::string& filepath) : running(true) {
    log_file.open(filepath, std::ios::out | std::ios::trunc);
    if (!log_file.is_open()) {
        std::cerr << "[ERROR] Could not open log file: " << filepath << "\n";
    } else {
        log_file << "Timestamp(ns),BuyerID,SellerID,Price,Quantity\n";
    }
    
    background_thread = std::thread(&Logger::process_logs, this);
}

Logger::~Logger() {
    running.store(false, std::memory_order_release);
    
    if (background_thread.joinable()) {
        background_thread.join();
    }
    if (log_file.is_open()) {
        log_file.close();
    }
}

void Logger::log_trade(uint64_t buyer_id, uint64_t seller_id, uint32_t price, uint32_t qty) {
    TradeLog log;
    log.buyer_order_id = buyer_id;
    log.seller_order_id = seller_id;
    log.price = price;
    log.quantity = qty;
    log.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // CRITICAL FIX: Handle the case where the logging ring buffer is full.
    // Spin-wait using __builtin_ia32_pause() to keep the CPU active but 
    // prevent the matching engine from dropping logs or crashing.
    while (!ring_buffer.push(log)) {
        __builtin_ia32_pause();
    }
}

void Logger::process_logs() {
    TradeLog log;
    
    // 1. Hot path loop: Read and write logs as long as the engine is running
    while (running.load(std::memory_order_acquire)) {
        
        while (ring_buffer.pop(log)) {
            if (log_file.is_open()) {
                log_file << log.timestamp << ","
                         << log.buyer_order_id << ","
                         << log.seller_order_id << ","
                         << log.price << ","
                         << log.quantity << "\n";
            }
        }
        
        // CRITICAL HFT OPTIMIZATION:
        // Replace std::this_thread::yield() to prevent the OS scheduler from 
        // putting this thread to sleep for too long, causing a buffer overflow.
        __builtin_ia32_pause();
    }

    // 2. Flush/Drain path: Ensure remaining trades in the buffer are saved 
    // to disk during application shutdown.
    while (ring_buffer.pop(log)) {
        if (log_file.is_open()) {
            log_file << log.timestamp << ","
                     << log.buyer_order_id << ","
                     << log.seller_order_id << ","
                     << log.price << ","
                     << log.quantity << "\n";
        }
    }
}