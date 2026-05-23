# NANOMATCH -- Ultra-Low Latency Order Matching Engine

> A fully functional C++17 Limit Order Book (LOB) engineered from the ground up for sub-microsecond, deterministic execution. This project bridges the gap between theoretical quantitative finance and hardcore, low-level systems engineering by stripping away high-level abstractions and building cache-aligned, contiguous data structures that mirror the architectural mindset demanded by top-tier proprietary trading firms.

---

## Table of Contents

1. [Problem Statement](#problem-statement)
2. [Project Goals](#project-goals)
3. [Architecture and Design Decisions](#architecture-and-design-decisions)
4. [Implementation Evolution](#implementation-evolution)
5. [Tech Stack](#tech-stack)
6. [Benchmark Results](#benchmark-results)
7. [Hardware Specifications](#hardware-specifications)
8. [Reproducing Benchmark Results](#reproducing-benchmark-results)
9. [Running the Engine](#running-the-engine)

---

## Problem Statement

Standard algorithmic trading strategies are written in high-level languages, focusing purely on alpha generation and mathematical models. However, in the HFT industry, the most brilliant alpha model is completely useless if the underlying system executing the trade is a microsecond too slow. The absolute core of any financial exchange -- and the primary battleground for quantitative systems engineers -- is the Order Matching Engine.

The true challenge of this project is not just implementing the matching logic, but executing that logic with absolute hardware sympathy. Standard C++ practices heavily utilized in competitive programming (like `std::map` or dynamic memory allocations) introduce unpredictable latency spikes due to operating system overhead and CPU cache misses. By stripping away these high-level abstractions to build cache-aligned, contiguous data structures, this engine demonstrates deterministic, sub-microsecond execution.

---

## Project Goals

- Build a Core Limit Order Book that processes Buy/Sell limit orders, market orders, and cancellations on strict price-time priority.
- Implement Cache-Optimized Memory Management by replacing STL containers with custom memory pools and contiguous arrays to eliminate OS overhead and L1/L2 cache misses.
- Develop a High-Throughput Data Ingestion Pipeline that parses multi-million row CSVs using zero-copy `mmap`.
- Integrate Lock-Free Concurrency via an asynchronous SPSC ring buffer using atomic operations for trade logging without blocking the main execution thread.
- Establish a Rigorous Benchmarking Harness using Google Benchmark, reporting throughput, p50, p90, and p99 latencies.

---

## Architecture and Design Decisions

### 1. Flat Array Price Ladder Over `std::map`

The most critical architectural decision is the representation of the order book itself.

**The problem with `std::map`:** A `std::map<uint64_t, PriceLevel>` stores its nodes in heap-allocated, non-contiguous memory connected by pointers. Every lookup traverses a red-black tree, causing a chain of pointer dereferences across disparate memory locations. With a market depth of 10,000 price levels, each `add_order` or `cancel_order` call is virtually guaranteed to cause multiple L1/L2 cache misses, each costing 100+ nanoseconds on modern hardware.

**The solution -- a flat array price ladder:** The optimized engine (`src/`) allocates two contiguous `std::vector<PriceLevel>` arrays -- `bids` and `asks` -- with a fixed `MAX_PRICE` capacity (default: 1,000,000). A price `p` maps directly and instantly to `bids[p]` or `asks[p]` with O(1) array indexing, zero pointer indirection, and maximum cache locality. Accessing a price level that was recently touched is nearly guaranteed to be a cache hit.

```cpp
// Optimized: O(1) direct array access, cache-friendly
std::vector<PriceLevel> bids; // bids[price] -> PriceLevel
std::vector<PriceLevel> asks; // asks[price] -> PriceLevel
```

### 2. Bitboard-Accelerated Best Bid/Ask Tracking

**The problem:** After a price level is depleted, the engine must find the next best bid (next highest price) or next best ask (next lowest price). Scanning the flat array linearly is O(MAX_PRICE), which is catastrophically slow.

**The solution -- a bitboard:** The engine maintains two `std::vector<uint64_t>` bitboards (`bid_bitboard`, `ask_bitboard`). Each bit at position `p` is `1` if that price level is active and `0` otherwise. Finding the next best price reduces to a single hardware-accelerated bit-scan operation using `__builtin_clzll` (count leading zeros) and `__builtin_ctzll` (count trailing zeros). These compile to a single CPU instruction (`BSR`/`BSF` on x86), making best-price discovery effectively O(1) in hardware.

```cpp
// Finding the next best ask: a single instruction on the CPU
best_ask = (block_idx * 64) + __builtin_ctzll(ask_bitboard[block_idx]);
```

### 3. Custom Memory Pool -- Eliminating `new` and `delete`

**The problem:** Every call to `new` or `malloc` is a system call that goes through the OS memory allocator. This is non-deterministic, can block, causes heap fragmentation, and is the single largest source of latency jitter in high-frequency systems.

**The solution -- a pre-allocated slab allocator:** `MemoryPool` pre-allocates a contiguous block of 1,000,000 `Order` objects on startup in a single `std::vector<Order> pool`. A free list (`std::vector<int32_t> free_list`) tracks available slots by index. `allocate()` and `deallocate()` are simple `pop_back`/`push_back` operations on the free list -- no OS interaction, no fragmentation, and guaranteed sub-nanosecond allocation.

```cpp
// O(1) allocation -- just a vector pop_back(), no system calls
int32_t allocate() {
    int32_t index = free_list.back();
    free_list.pop_back();
    return index;
}
```

All order nodes are linked using integer indices (int32_t `next_index`, `prev_index`) into this pool rather than raw pointers, making the doubly-linked list at each price level entirely contained within the pool's contiguous memory region.

### 4. Zero-Copy mmap Data Ingestion

**The problem:** Using `std::ifstream` to read a CSV file issues repeated `read()` system calls, copying data from the kernel's page cache into a userspace buffer. For a multi-million row order file, this incurs significant system call overhead and redundant memory copies.

**The solution -- `mmap`:** `CSVReader` uses `mmap(MAP_PRIVATE)` to map the entire file directly into the process's virtual address space. The OS page cache becomes the buffer. The parser (`parse_uint`) then operates directly on the mapped memory using a raw `const char*` cursor, achieving true zero-copy parsing with no intermediate buffers and no system call overhead per record.

### 5. Lock-Free SPSC Ring Buffer

**The problem:** The trade logger cannot write to disk on the critical matching path. Disk I/O is orders of magnitude slower than in-memory operations and would stall the matching engine.

**The solution -- a Single Producer, Single Consumer (SPSC) ring buffer:** `SPSCRingBuffer<T, Size>` is a lock-free queue designed for exactly one producer (the matching engine) and one consumer (the logger background thread). It uses two `alignas(64)` atomic indices (`write_index`, `read_index`) to completely avoid mutexes and condition variables. The `alignas(64)` directive places each index on its own cache line, preventing false sharing -- a subtle but critical optimization where two threads writing to different variables on the same cache line cause constant cache invalidation between CPU cores.

The matching engine calls `ring_buffer.push()` with `memory_order_release` and the logger thread calls `ring_buffer.pop()` with `memory_order_acquire`, establishing the correct happens-before relationship with the minimum possible memory barrier overhead.

The ring buffer size is constrained to be a power of 2 (enforced by `static_assert`), allowing the modulo wrapping to be replaced with a single bitwise AND:

```cpp
size_t next_write = (current_write + 1) & (Size - 1); // No expensive modulo
```

### 6. TCP Server with Binary Protocol and `TCP_NODELAY`

For the live trading mode, the `TCPServer` accepts binary order packets over TCP. The packet format uses `#pragma pack(push, 1)` to produce a tightly-packed 18-byte struct with no padding, minimizing wire bytes. `TCP_NODELAY` (disabling Nagle's algorithm) is set on every accepted client socket to prevent the OS from buffering small packets and introducing artificial latency.

### 7. `alignas(64)` on the Order Struct

The `Order` struct in `src/order.h` is annotated with `alignas(64)`, aligning it to a full CPU cache line boundary. This ensures that a single `Order` object never straddles two cache lines, which would require two cache line fetches to read a single order and could cause false sharing between adjacent orders in the pool.

---

## Implementation Evolution

The project follows a deliberate three-stage evolution to quantify the real-world impact of each optimization.

### Stage 1 -- Baseline (`src_baseline/`)

The simplest correct implementation. Bids and asks are stored as `std::vector<Order>`, kept sorted via `std::sort` after every insertion. Cancellation is a linear O(N) scan with `std::find_if`. This is the standard approach a software generalist might reach for.

- **Add Order:** O(N log N) due to `std::sort`
- **Cancel Order:** O(N) linear scan
- **Memory:** Dynamic heap allocations per operation

### Stage 2 -- Map-Based (`src_mapBased/`)

A significant improvement using `std::map<uint64_t, PriceLevel>` (a red-black tree) for O(log N) price level lookup. Each `PriceLevel` holds a `std::list<Order>` for O(1) front-of-queue matching. An `std::unordered_map` provides O(1) average cancellation by mapping `order_id` to a list iterator.

- **Add Order:** O(log N) tree traversal, heap allocations for list nodes
- **Cancel Order:** O(1) average with hash map, but with heap-pointer indirection and potential hash collisions
- **Memory:** Non-contiguous, pointer-chased heap nodes -- poor cache locality

### Stage 3 -- Optimized (`src/`)

The production-grade engine. Replaces all dynamic, pointer-based containers with flat arrays, a custom memory pool, a bitboard, and a lock-free logger. Every operation is designed to be cache-resident and branch-predictable.

- **Add Order:** O(1) array index, O(1) pool allocation, O(1) bitboard update
- **Cancel Order:** O(1) direct `order_map` lookup by `order_id`, O(1) pool deallocation, O(1) bitboard update
- **Memory:** Fully pre-allocated, contiguous, cache-line aligned

---

## Tech Stack

| Component | Technology |
|---|---|
| Language | C++17 |
| Build System | CMake 3.15+ |
| Benchmarking | Google Benchmark v1.8.3 |
| Profiling | Linux `perf`, Intel VTune |
| Concurrency | `std::atomic`, `std::thread` |
| I/O | `mmap`, POSIX TCP sockets |
| Data | Synthetic CSV via Python generator |

---

## Benchmark Results

The logger was disabled in the optimized engine to ensure a fair comparison of the matching core only.

All timings are in nanoseconds (ns). Each benchmark runs 100 repetitions against a pre-filled book of 10,000 resting orders. Each iteration measures one `add_order` and one `cancel_order` call. The logger was disabled in the optimized engine to ensure a fair comparison of the matching core only.
 
**Time** is wall-clock time. **CPU** is the time the processor was actually executing benchmark code, excluding OS scheduling preemptions. For latency analysis, CPU time is the more meaningful number.
 
| Implementation | Mean Time (ns) | Mean CPU (ns) | Median Time (ns) | Median CPU (ns) | Std Dev (ns) | CV | p50 (ns) | p90 (ns) | p99 (ns) |
|---|---|---|---|---|---|---|---|---|---|
| Baseline (vector + sort) | 374,200 | 374,700 | 339,300 | 339,700 | 65,500 | 17.51% | 339,200 | 481,100 | 546,400 |
| Map-Based (std::map + list) | 1,246 | 1,261 | 1,245 | 1,261 | 144 | 11.55% | 1,243 | 1,431 | 1,595 |
| Optimized (flat array + bitboard) | 511 | 516 | 502 | 506 | 41 | 7.92% | 502 | 559 | 643 |
 
**Key takeaways:**
 
- The optimized engine is **~2.4x faster** than the map-based implementation at median latency (502 ns vs 1,245 ns).
- The optimized engine is **~676x faster** than the baseline at median latency (502 ns vs 339,276 ns).
- The optimized engine has the lowest CV (7.92%), meaning it is the most consistent and deterministic -- a critical property for HFT systems where tail latency is as important as average latency.
- The baseline's high CV (17.51%) reflects the unpredictable cost of `std::sort` and linear scans as the book depth varies across iterations.

---

## Hardware Specifications

Benchmarks were run on the following hardware:

```
Run on (8 X 2112.01 MHz CPUs)
CPU Caches:
  L1 Data        32 KiB (x4)
  L1 Instruction 32 KiB (x4)
  L2 Unified    256 KiB (x4)
  L3 Unified   6144 KiB (x1)
```

The L1 data cache size is particularly relevant to this benchmark. The optimized engine is designed so that the hot path data -- the active price levels near best bid/ask, the bitboard blocks, and the order pool's free list -- fits comfortably within the 32 KiB L1 cache during steady-state operation.

---

## Reproducing Benchmark Results

### Prerequisites

- GCC or Clang with C++17 support
- CMake 3.15+
- Git (for Google Benchmark FetchContent)
- Python 3 (for data generation and the trading bot)

### Step 1 -- Clone and Generate Data

```bash
# Generate synthetic order data
mkdir -p data
python generate_data.py
```

### Step 2 -- Build

```bash
rm -rf build/
mkdir build
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

### Step 3 -- Run Benchmarks

```bash
./build/nanomatch_bench
```

### Step 4 -- Run the Optimized Engine (CSV Mode)

```bash
./build/nanomatch
```

Reads orders from `data/orders.csv` using zero-copy `mmap`, processes them through the matching engine, and prints the top 5 levels of the order book ladder on completion.

### Step 5 -- Run the Optimized Engine (Live Mode)

In terminal 1:

```bash
./build/nanomatch live
```

In terminal 2 (split):

```bash
python trader.py
```

The trading bot connects over TCP on port 8080 and streams 100,000 binary-encoded orders in batched transmissions. The matching engine processes them in real time via the SPSC ring buffer.

### Step 6 -- Run Standalone Implementations (Testing Only)

```bash
# Baseline implementation
./build/run_baseline

# Map-based implementation
./build/run_map
```

These run the predefined scenario tests for each implementation. No data ingestion pipeline is implemented for these -- they are for correctness verification only.

---

## Concepts Demonstrated

- Cache locality -- L1/L2/L3 vs RAM hierarchy
- Custom memory pools and arenas
- Lock-free concurrency and memory ordering
- Zero-copy I/O via `mmap`
- Struct packing and false sharing prevention
- Branch misprediction and pipeline hazards
- Bitwise operations for hardware-accelerated search