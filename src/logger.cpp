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

    while (!ring_buffer.push(log)) {
        __builtin_ia32_pause();
    }
}

void Logger::process_logs() {
    TradeLog log;
    
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
        
        __builtin_ia32_pause();
    }

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