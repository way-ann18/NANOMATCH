#include "orderbook.h"
#include <iostream>

int main() {
    OrderBook book;
    uint64_t current_time = 1000;

    std::cout << "--- NANOMATCH V1: INITIALIZED ---\n\n";

    // 1. Build the Order Book
    book.add_order({1, current_time++, 10040, 10, Side::BUY, OrderType::LIMIT});
    book.add_order({2, current_time++, 10050, 15, Side::BUY, OrderType::LIMIT});
    book.add_order({3, current_time++, 10050, 5,  Side::BUY, OrderType::LIMIT}); 
    book.add_order({4, current_time++, 10070, 20, Side::SELL, OrderType::LIMIT});
    
    std::cout << "\n--- Book Before Cancellation ---\n";
    book.print_book();

    // 2. Cancel Order #2 (The 15 qty Buy order at 10050)
    std::cout << ">>> INCOMING: Cancel Request for Order ID 2\n";
    book.cancel_order(2);

    std::cout << "\n--- Book After Cancellation ---\n";
    book.print_book();

    // 3. Introduce the massive MARKET SELL order
    std::cout << ">>> INCOMING: Massive Market Sell Order (Qty: 25)\n";
    book.add_order({5, current_time++, 0, 25, Side::SELL, OrderType::MARKET});

    book.print_book();

    return 0;
}