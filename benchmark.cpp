#include <benchmark/benchmark.h>
#include <random>
#include <vector>
#include <algorithm>

// Project Headers
#include "memory_pool.h"
#include "orderbook.h"               // O(1) Engine
#include "../src_baseline/orderbook.h" // Baseline Engine
#include "../src_mapBased/orderbook.h" // Map Engine

// --- UNIFIED BENCHMARK CONSTANTS ---
const int MARKET_DEPTH = 10000;
const int PREALLOC_SIZE = 100000;

// Helper for custom percentiles
double calc_percentile(const std::vector<double>& v, double p) {
    if (v.empty()) return 0.0;
    std::vector<double> copy = v;
    std::sort(copy.begin(), copy.end());
    size_t idx = (p / 100.0) * (copy.size() - 1);
    return copy[idx];
}

// ==========================================
// 1. OPTIMIZED O(1) ENGINE FIXTURE
// ==========================================
class OrderBookFixture : public benchmark::Fixture {
public:
    MemoryPool* pool; Logger* logger; OrderBook* book;
    std::vector<uint64_t> resting_ids;

    void SetUp(const ::benchmark::State& state) {
        pool = new MemoryPool();
        logger = new Logger("/dev/null");
        book = new OrderBook(*pool, *logger); // Defaults to max 2,000,000
        resting_ids.resize(MARKET_DEPTH);
        
        // Pre-fill to steady state (IDs 1 to 10,000)
        for(int i = 0; i < MARKET_DEPTH; ++i) {
            Order o; 
            o.order_id = i + 1; 
            o.side = (i % 2 == 0) ? Side::BUY : Side::SELL; 
            o.type = OrderType::LIMIT;
            o.price = 10000 + (i % 500); 
            o.quantity = 10;
            book->add_order(o);
            resting_ids[i] = o.order_id; // Store in resting array
        }
    }
    void TearDown(const ::benchmark::State& state) { delete book; delete logger; delete pool; }
};

BENCHMARK_F(OrderBookFixture, OptimizedLatency)(benchmark::State& state) {
    std::mt19937 rng(1337);
    
    // Pre-calculate random inputs to keep the timed loop pure
    std::vector<uint64_t> new_ids(PREALLOC_SIZE);
    std::vector<uint32_t> cancel_indices(PREALLOC_SIZE);
    std::vector<uint32_t> random_prices(PREALLOC_SIZE);
    
    for(int i = 0; i < PREALLOC_SIZE; ++i) {
        new_ids[i] = 100000 + i; // Safe unique IDs: 100,000 to 199,999
        cancel_indices[i] = rng() % MARKET_DEPTH; 
        random_prices[i] = 10000 + (rng() % 1000); 
    }
    
    size_t i = 0;
    for (auto _ : state) {
        size_t mod_i = i % PREALLOC_SIZE;
        
        Order order;
        order.order_id = new_ids[mod_i];
        order.side = Side::BUY;
        order.type = OrderType::LIMIT;
        order.price = random_prices[mod_i];
        order.quantity = 10;
        
        // 1. TIMED: Add the order
        book->add_order(order);
        
        // 2. PAUSED: Benchmark Management Overhead
        state.PauseTiming(); 
        size_t idx_to_cancel = cancel_indices[mod_i];
        uint64_t id_to_cancel = resting_ids[idx_to_cancel];
        resting_ids[idx_to_cancel] = order.order_id; // Swap ID in array
        state.ResumeTiming();
        
        // 3. TIMED: Cancel the old resting order
        book->cancel_order(id_to_cancel); 
        
        i++;
    }
}

// ==========================================
// 2. BASELINE FIXTURE
// ==========================================
class BaselineFixture : public benchmark::Fixture {
public:
    baseline::OrderBook* baseline_book;
    std::vector<uint64_t> resting_ids;
    
    void SetUp(const ::benchmark::State& state) { 
        baseline_book = new baseline::OrderBook(); 
        resting_ids.resize(MARKET_DEPTH);
        
        for(int i = 0; i < MARKET_DEPTH; ++i) {
            baseline::Order o;
            o.order_id = i + 1;
            o.side = (i % 2 == 0) ? baseline::Side::BUY : baseline::Side::SELL;
            o.type = baseline::OrderType::LIMIT;
            o.price = 10000 + (i % 500);
            o.quantity = 10;
            baseline_book->add_order(o);
            resting_ids[i] = o.order_id;
        }
    }
    void TearDown(const ::benchmark::State& state) { delete baseline_book; }
};

BENCHMARK_F(BaselineFixture, BaselineLatency)(benchmark::State& state) {
    std::mt19937 rng(1337);
    std::vector<uint64_t> new_ids(PREALLOC_SIZE);
    std::vector<uint32_t> cancel_indices(PREALLOC_SIZE);
    std::vector<uint32_t> random_prices(PREALLOC_SIZE);
    
    for(int i = 0; i < PREALLOC_SIZE; ++i) {
        new_ids[i] = 100000 + i; 
        cancel_indices[i] = rng() % MARKET_DEPTH; 
        random_prices[i] = 10000 + (rng() % 1000); 
    }
    
    size_t i = 0;
    for (auto _ : state) {
        size_t mod_i = i % PREALLOC_SIZE;
        
        baseline::Order order;
        order.order_id = new_ids[mod_i];
        order.side = baseline::Side::BUY;
        order.type = baseline::OrderType::LIMIT;
        order.price = random_prices[mod_i];
        order.quantity = 10;
        
        baseline_book->add_order(order);
        
        state.PauseTiming();
        size_t idx_to_cancel = cancel_indices[mod_i];
        uint64_t id_to_cancel = resting_ids[idx_to_cancel];
        resting_ids[idx_to_cancel] = order.order_id; 
        state.ResumeTiming();
        
        baseline_book->cancel_order(id_to_cancel); 
        
        i++;
    }
}

// ==========================================
// 3. MAP-BASED FIXTURE
// ==========================================
class MapBasedFixture : public benchmark::Fixture {
public:
    map_queue::MapOrderBook* map_book;
    std::vector<uint64_t> resting_ids;
    
    void SetUp(const ::benchmark::State& state) { 
        map_book = new map_queue::MapOrderBook(); 
        resting_ids.resize(MARKET_DEPTH);
        
        for(int i = 0; i < MARKET_DEPTH; ++i) {
            map_queue::Order o;
            o.order_id = i + 1;
            o.is_buy = (i % 2 == 0);
            o.price = 10000 + (i % 500);
            o.quantity = 10;
            map_book->add_order(o);
            resting_ids[i] = o.order_id;
        }
    }
    void TearDown(const ::benchmark::State& state) { delete map_book; }
};

BENCHMARK_F(MapBasedFixture, MapLatency)(benchmark::State& state) {
    std::mt19937 rng(1337);
    std::vector<uint64_t> new_ids(PREALLOC_SIZE);
    std::vector<uint32_t> cancel_indices(PREALLOC_SIZE);
    std::vector<uint32_t> random_prices(PREALLOC_SIZE);
    
    for(int i = 0; i < PREALLOC_SIZE; ++i) {
        new_ids[i] = 100000 + i; 
        cancel_indices[i] = rng() % MARKET_DEPTH; 
        random_prices[i] = 10000 + (rng() % 1000); 
    }
    
    size_t i = 0;
    for (auto _ : state) {
        size_t mod_i = i % PREALLOC_SIZE;
        
        map_queue::Order order;
        order.order_id = new_ids[mod_i];
        order.is_buy = true;
        order.price = random_prices[mod_i];
        order.quantity = 10;
        
        map_book->add_order(order);
        
        state.PauseTiming();
        size_t idx_to_cancel = cancel_indices[mod_i];
        uint64_t id_to_cancel = resting_ids[idx_to_cancel];
        resting_ids[idx_to_cancel] = order.order_id; 
        state.ResumeTiming();
        
        map_book->cancel_order(id_to_cancel); 
        
        i++;
    }
}



// ==========================================
// REGISTRATION
// ==========================================
#define REGISTER_BENCHMARK(fixture, name) \
BENCHMARK_REGISTER_F(fixture, name) \
    ->Unit(benchmark::kNanosecond) ->Repetitions(100) ->DisplayAggregatesOnly(true) \
    ->ComputeStatistics("p50", [](const std::vector<double>& v) { return calc_percentile(v, 50.0); }) \
    ->ComputeStatistics("p90", [](const std::vector<double>& v) { return calc_percentile(v, 90.0); }) \
    ->ComputeStatistics("p99", [](const std::vector<double>& v) { return calc_percentile(v, 99.0); });

REGISTER_BENCHMARK(OrderBookFixture, OptimizedLatency);
REGISTER_BENCHMARK(BaselineFixture, BaselineLatency);
REGISTER_BENCHMARK(MapBasedFixture, MapLatency);

BENCHMARK_MAIN();