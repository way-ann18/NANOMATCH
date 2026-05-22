#include "logger.h"
#include <iostream>
#include <chrono>

Logger::Logger(const std::string& filepath) : running(true) {
    log_file.open(filepath, std::ios::out | std::ios::trunc);
    if (!log_file.is_open()) {
        std::cerr << "[ERROR] Could not open log file: " << filepath << "\n";
    } else {
        // Write the CSV header
        log_file << "Timestamp(ns),BuyerID,SellerID,Price,Quantity\n";
    }
    
    // Spin up the background thread immediately
    background_thread = std::thread(&Logger::process_logs, this);
}

Logger::~Logger() {
    // Tell the background thread to shut down
    running.store(false, std::memory_order_release);
    
    // Wait for the background thread to finish writing its final logs
    if (background_thread.joinable()) {
        background_thread.join();
    }
    if (log_file.is_open()) {
        log_file.close();
    }
}

// ============================================================================
// MAIN THREAD: Drop the trade in the buffer in O(1) time and escape instantly
// ============================================================================
void Logger::log_trade(uint64_t buyer_id, uint64_t seller_id, uint32_t price, uint32_t qty) {
    TradeLog log;
    log.buyer_order_id = buyer_id;
    log.seller_order_id = seller_id;
    log.price = price;
    log.quantity = qty;
    log.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // The entire lock-free memory barrier math is handled by the RingBuffer class!
    ring_buffer.push(log);
}

// ============================================================================
// BACKGROUND THREAD: Loop infinitely, writing to the hard drive
// ============================================================================
void Logger::process_logs() {
    TradeLog log;
    
    // Keep looping while the engine is running
    while (running.load(std::memory_order_acquire)) {
        
        // Drain the buffer as fast as possible
        while (ring_buffer.pop(log)) {
            if (log_file.is_open()) {
                log_file << log.timestamp << ","
                         << log.buyer_order_id << ","
                         << log.seller_order_id << ","
                         << log.price << ","
                         << log.quantity << "\n";
            }
        }
        
        // If the buffer is empty, yield the CPU core so we don't burn 100% CPU usage
        std::this_thread::yield();
    }

    // --- SHUTDOWN SEQUENCE ---
    // The engine told us to stop, but there might still be trades stuck in the buffer.
    // Do one final flush before killing the thread completely.
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