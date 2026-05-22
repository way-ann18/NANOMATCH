#include <benchmark/benchmark.h>
#include "memory_pool.h"
#include "orderbook.h"
#include "logger.h"
#include "../src_baseline/orderbook.h" // Your baseline namespace
#include <vector>
#include <algorithm>

// --- Helper for Percentile Math ---
double calc_percentile(const std::vector<double>& v, double p) {
    if (v.empty()) return 0.0;
    std::vector<double> copy = v;
    std::sort(copy.begin(), copy.end());
    size_t idx = (p / 100.0) * (copy.size() - 1);
    return copy[idx];
}

// =======================================================================
// 1. OPTIMIZED ENGINE FIXTURE (O(1) Memory Pool & Bit-Scanning)
// =======================================================================
class OrderBookFixture : public benchmark::Fixture {
public:
    MemoryPool* pool;
    Logger* logger;
    OrderBook* book;

    void SetUp(const ::benchmark::State& state) {
        pool = new MemoryPool();
        logger = new Logger("/dev/null"); // Drop logs to the void
        book = new OrderBook(*pool, *logger);

        // --- PRE-FILL: 5000 RESTING BUYERS ---
        Order dummy_bid;
        dummy_bid.quantity = 100;
        dummy_bid.side = Side::BUY;
        dummy_bid.type = OrderType::LIMIT;
        for (int i = 0; i < 5000; ++i) {
            dummy_bid.order_id = 1000000 + i;
            dummy_bid.price = 10400 - (i % 500); 
            book->add_order(dummy_bid);
        }

        // --- PRE-FILL: 5000 RESTING SELLERS ---
        Order dummy_ask;
        dummy_ask.quantity = 100;
        dummy_ask.side = Side::SELL;
        dummy_ask.type = OrderType::LIMIT;
        // Change it from 2000000 to 1500000
        for (int i = 0; i < 5000; ++i) {
            dummy_ask.order_id = 1500000 + i; // <-- FIXED
            dummy_ask.price = 10600 + (i % 500); 
            book->add_order(dummy_ask);
        }
    }

    void TearDown(const ::benchmark::State& state) {
        delete book;
        delete logger;
        delete pool;
    }
};

BENCHMARK_F(OrderBookFixture, MatchLatency)(benchmark::State& state) {
    Order order;
    order.price = 10500; // Right in the middle of the spread
    order.quantity = 10;
    order.side = Side::BUY;
    order.type = OrderType::LIMIT;
    
    uint64_t id = 100000; // Use a safe, static ID

    for (auto _ : state) {
        order.order_id = id;
        
        book->add_order(order);
        
        state.PauseTiming();
        book->cancel_order(id);
        // id++;  <-- DELETE THIS LINE
        state.ResumeTiming();
    }
}

// =======================================================================
// 2. BASELINE ENGINE FIXTURE (Standard STL Vector/Sort)
// =======================================================================
class BaselineFixture : public benchmark::Fixture {
public:
    baseline::OrderBook* baseline_book;

    void SetUp(const ::benchmark::State& state) {
        baseline_book = new baseline::OrderBook();

        // --- PRE-FILL: 5000 RESTING BUYERS ---
        baseline::Order dummy_bid;
        dummy_bid.quantity = 100;
        dummy_bid.side = baseline::Side::BUY;
        dummy_bid.type = baseline::OrderType::LIMIT;
        for (int i = 0; i < 5000; ++i) {
            dummy_bid.order_id = 1000000 + i;
            dummy_bid.price = 10400 - (i % 500);
            baseline_book->add_order(dummy_bid);
        }

        // --- PRE-FILL: 5000 RESTING SELLERS ---
        baseline::Order dummy_ask;
        dummy_ask.quantity = 100;
        dummy_ask.side = baseline::Side::SELL;
        dummy_ask.type = baseline::OrderType::LIMIT;
        for (int i = 0; i < 5000; ++i) {
            dummy_ask.order_id = 1500000 + i;
            dummy_ask.price = 10600 + (i % 500);
            baseline_book->add_order(dummy_ask);
        }
    }

    void TearDown(const ::benchmark::State& state) {
        delete baseline_book;
    }
};

BENCHMARK_F(BaselineFixture, BaselineLatency)(benchmark::State& state) {
    baseline::Order order;
    order.price = 10500; // Right in the middle of the spread
    order.quantity = 10;
    order.side = baseline::Side::BUY;
    order.type = baseline::OrderType::LIMIT;
    uint64_t id = 1;

    for (auto _ : state) {
        order.order_id = id;
        
        baseline_book->add_order(order);
        
        state.PauseTiming();
        baseline_book->cancel_order(id);
        id++;
        state.ResumeTiming();
    }
}

// =======================================================================
// 3. REGISTRATION & MAIN
// =======================================================================
BENCHMARK_REGISTER_F(OrderBookFixture, MatchLatency)
    ->Unit(benchmark::kNanosecond)
    ->Repetitions(100)
    ->DisplayAggregatesOnly(true)
    ->ComputeStatistics("p50", [](const std::vector<double>& v) { return calc_percentile(v, 50.0); })
    ->ComputeStatistics("p90", [](const std::vector<double>& v) { return calc_percentile(v, 90.0); })
    ->ComputeStatistics("p99", [](const std::vector<double>& v) { return calc_percentile(v, 99.0); });

BENCHMARK_REGISTER_F(BaselineFixture, BaselineLatency)
    ->Unit(benchmark::kNanosecond)
    ->Repetitions(100)
    ->DisplayAggregatesOnly(true)
    ->ComputeStatistics("p50", [](const std::vector<double>& v) { return calc_percentile(v, 50.0); })
    ->ComputeStatistics("p90", [](const std::vector<double>& v) { return calc_percentile(v, 90.0); })
    ->ComputeStatistics("p99", [](const std::vector<double>& v) { return calc_percentile(v, 99.0); });

BENCHMARK_MAIN();