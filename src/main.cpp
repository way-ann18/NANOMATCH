#include "memory_pool.h"
#include "orderbook.h"
#include "csv_reader.h"
#include "tcp_server.h"
#include <iostream>
#include <chrono>
#include <string>

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "          NANOMATCH HFT ENGINE          \n";
    std::cout << "========================================\n\n";
    Logger logger("data/trades.csv");
    MemoryPool pool;
    OrderBook book(pool, logger);

    bool run_live = (argc > 1 && std::string(argv[1]) == "live");

    if (run_live) {
        
        TCPServer server(8080, book);
        server.start_listening(); 
        
    } else {
        auto start = std::chrono::high_resolution_clock::now();

        CSVReader::load_orders("data/orders.csv", book);

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        std::cout << "\n========================================\n";
        std::cout << "[BENCHMARK] Total Execution Time: " << duration.count() << " microseconds.\n";
        std::cout << "========================================\n";

        book.print_book();
    }

    return 0;
}