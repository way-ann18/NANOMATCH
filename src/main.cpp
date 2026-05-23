#include "memory_pool.h"
#include "orderbook.h"
#include "csv_reader.h"
#include "tcp_server.h"
#include "ring_buffer.h"
#include <iostream>
#include <chrono>
#include <string>

void matching_engine_worker(SPSCRingBuffer<Order, 131072>& buffer, OrderBook& book, std::atomic<bool>& running) {
    Order order;
    while (running) {
        if (buffer.pop(order)) {
            book.add_order(order);
        }
        __builtin_ia32_pause(); 
    }
}

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "          NANOMATCH HFT ENGINE          \n";
    std::cout << "========================================\n\n";
    Logger logger("data/trades.csv");
    MemoryPool pool;
    OrderBook book(pool, logger);

    bool run_live = (argc > 1 && std::string(argv[1]) == "live");

    if (run_live) {
        SPSCRingBuffer<Order, 131072> buffer;
        std::atomic<bool> running{true};
        std::thread engine_thread(matching_engine_worker, std::ref(buffer), std::ref(book), std::ref(running));
        TCPServer server(8080, buffer);
        server.start_listening(); 
        running = false;
        engine_thread.join();
        
    } else {

        CSVReader::load_orders("data/orders.csv", book);
        book.print_book();
    }

    return 0;
}