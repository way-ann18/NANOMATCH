#include "orderbook.h"
#include <iostream>
#include <chrono>

int main() {
    baseline::OrderBook book;
    uint64_t current_time = 1000;

    std::cout << "--- NANOMATCH BASELINE: CORE ENGINE BENCHMARK ---\n\n";

    book.add_order({1, current_time++, 10040, 10, baseline::Side::BUY, baseline::OrderType::LIMIT});
    book.add_order({2, current_time++, 10050, 15, baseline::Side::BUY, baseline::OrderType::LIMIT});
    book.add_order({3, current_time++, 10050, 5,  baseline::Side::BUY, baseline::OrderType::LIMIT}); 
    book.add_order({4, current_time++, 10070, 20, baseline::Side::SELL, baseline::OrderType::LIMIT});
    
    std::cout << "--- Baseline Engine Test Complete ---\n";
    book.print_book();

    return 0;
}