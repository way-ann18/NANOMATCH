# NANOMATCH — Ultra-Low Latency Order Matching Engine

> **C++17 · CMake · Google Benchmark · Linux perf · std::atomic**
>
> A fully functional Limit Order Book (LOB) engineered for sub-microsecond execution latency. Built by stripping away the C++ Standard Library in the critical path and replacing it with cache-aligned contiguous data structures, hardware bit-scan intrinsics, and a lock-free asynchronous logging pipeline.

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Architecture & Design Decisions](#architecture--design-decisions)
3. [Directory Structure](#directory-structure)
4. [Build Instructions](#build-instructions)
5. [Running the Engine](#running-the-engine)
6. [Google Benchmark — Latency Testing](#google-benchmark--latency-testing)
7. [Flame Graph Profiling (L1 Cache Misses)](#flame-graph-profiling-l1-cache-misses)
8. [Performance Results](#performance-results)
9. [Hardware Environment](#hardware-environment)

---

## Project Overview

Standard algorithmic trading strategies focus purely on alpha generation. In the HFT industry, the most brilliant alpha model is completely useless if the underlying system is a microsecond too slow. The absolute core of any financial exchange is the **Order Matching Engine**.

This project bridges the gap between quantitative finance theory and hardcore low-level systems engineering. It implements a complete, benchmarked, and profiled matching engine that demonstrates the exact architectural mindset demanded by top-tier proprietary trading firms.

**Core capabilities:**
- Processes Buy/Sell limit orders, market orders, and cancellations on strict price-time priority
- Replaces STL containers with a custom memory pool and contiguous arrays to eliminate OS overhead and L1/L2 cache misses
- Parses multi-million-row CSVs using zero-copy `mmap`
- Integrates a lock-free SPSC ring buffer for asynchronous trade logging without blocking the matching thread
- Reports throughput, p50 median, p90, and p99 tail latencies via Google Benchmark

---

## Architecture & Design Decisions

### 1. Pre-Faulted Memory Pool vs. STL Allocators

The baseline implementation stores orders in `std::vector<Order>` and looks them up via `std::map<uint64_t, Order*>`. This forces the OS to dynamically resize arrays during live trading (`_M_realloc_insert`) and causes the CPU to chase random pointer chains through a Red-Black tree — both resulting in severe L1 cache misses and hundreds of nanoseconds of stall time.

**NANOMATCH's solution (`src/memory_pool.h`):**
- Pre-allocates **1,000,000 order slots** in a single flat `std::vector<Order>` at startup
- Manages a free-list stack of available indices — `allocate()` and `deallocate()` are both O(1) pointer arithmetic
- Orders stored as **intrusive doubly-linked lists** where "pointers" are integer indices into this cache-hot contiguous array, eliminating pointer chasing entirely

### 2. O(1) Hardware Bit-Scan Execution

The baseline uses `std::sort` on a `std::vector` after every insert — O(N log N) per order. When the spread widens, walking the book degrades exponentially.

**NANOMATCH's solution (`src/orderbook.h`, `src/orderbook.cpp`):**
- Tracks active price levels using **64-bit integer bitboards** (`bid_bitboard`, `ask_bitboard`)
- Uses compiler intrinsics `__builtin_clzll` (count leading zeros) and `__builtin_ctzll` (count trailing zeros) to interact directly with the CPU's dedicated bit-scan silicon
- The CPU checks **64 price levels simultaneously in a single clock cycle**, reducing best-bid/best-ask lookup to strict **O(1)**

### 3. Lock-Free Asynchronous Logging

Writing to a file under a `std::mutex` triggers a `__sched_yield` context switch, forcing the OS to pause the matching thread mid-execution.

**NANOMATCH's solution (`src/ring_buffer.h`, `src/logger.h`, `src/logger.cpp`):**
- Implements an **SPSC (Single-Producer, Single-Consumer) ring buffer** templated on a power-of-2 size (1,048,576 slots)
- `alignas(64)` on the atomic read/write indices prevents **false sharing** between the producer (matching thread) and consumer (logger thread)
- Uses `std::memory_order_release` / `std::memory_order_acquire` for correct ordering without a mutex
- The matching thread pushes a 40-byte `TradeLog` struct and escapes in nanoseconds; a background thread drains the buffer and handles slow file I/O

### 4. Zero-Copy Data Ingestion

`src/csv_reader.cpp` uses Linux `mmap` to map the orders CSV directly into the process address space. A hand-rolled integer parser (`parse_uint`) reads raw bytes without `std::string` allocations or `std::stoi` overhead, eliminating the entire libc parsing stack from the hot path.

---

## Directory Structure

```
NANOMATCH/
│
├── src/
│   ├── main.cpp
│   ├── orderbook.h / .cpp
│   ├── order.h
│   ├── memory_pool.h
│   ├── ring_buffer.h
│   ├── logger.h / .cpp
│   ├── csv_reader.h / .cpp
│   ├── tcp_server.h / .cpp
│   └── benchmark.cpp
│
├── src_baseline/
│   ├── main.cpp
│   ├── order.h
│   └── orderbook.h / .cpp
│
├── src_mapBased/
│   ├── main.cpp
│   ├── order.h
│   └── orderbook.h / .cpp
│
├── CMakeLists.txt
├── generate_data.py
└── trader.py
```

---

## Build Instructions

### Prerequisites

```bash
sudo apt update
sudo apt install -y cmake g++ linux-tools-common linux-tools-generic git python3
```

> Google Benchmark is automatically fetched via CMake's `FetchContent` — no manual install needed.

### Compile

```bash
git clone https://github.com/yourusername/NANOMATCH.git
cd NANOMATCH
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

This produces two binaries inside `build/`:
- `NANOMATCH` — the main engine (backtester + live TCP mode)
- `nanomatch_bench` — the Google Benchmark latency harness

---

## Running the Engine

### Backtester Mode (mmap CSV ingestion)

First, generate a synthetic order dataset:

```bash
# From the project root
mkdir -p data
python3 generate_data.py
```

Then run the engine against the CSV:

```bash
./build/NANOMATCH
```

### Live Exchange Mode (TCP)

Start the engine in exchange mode:

```bash
./build/NANOMATCH live
```

In a separate terminal, launch the Python HFT bot to blast 100,000 binary orders over TCP:

```bash
python3 trader.py
```

The engine will process the incoming order stream and print the final order book state on disconnect.

---

## Google Benchmark — Latency Testing

The benchmark harness (`src/benchmark.cpp`) pre-fills both the optimized and baseline order books with **5,000 resting bids and 5,000 resting asks**, then measures the raw add/cancel latency over **100 repetitions** with p50, p90, and p99 statistics.

### Run (bare, no CPU pinning)

```bash
./build/nanomatch_bench
```

### Run (CPU pinned to Core 7 — recommended)

Pinning to a single physical core prevents the Linux scheduler from migrating the thread between cores and wiping the L1 cache mid-benchmark:

```bash
taskset -c 7 ./build/nanomatch_bench
```

### Run with custom output format

```bash
taskset -c 7 ./build/nanomatch_bench --benchmark_format=json > results.json
```

### Expected output columns

| Column | Description |
|--------|-------------|
| `mean` | Average latency across all repetitions |
| `median` / `p50` | 50th percentile latency |
| `p90` | 90th percentile tail latency |
| `p99` | 99th percentile worst-case latency |
| `stddev` | Standard deviation of latency |

---

## Flame Graph Profiling (L1 Cache Misses)

The flame graphs in the repository visually prove that the baseline STL algorithms dominate L1 cache miss events, while the optimized engine operates almost entirely within the L1 cache.

### Step 1 — Record hardware L1 cache-miss events

```bash
sudo perf record -e L1-dcache-load-misses -c 10000 -g -- ./build/nanomatch_bench
```

### Step 2 — Export the raw perf data

The `-f` flag forces perf to read despite WSL2 cross-drive user permission mismatches:

```bash
sudo perf script -f > out.perf
```

### Step 3 — Collapse stacks and generate the SVG

Clone Brendan Gregg's FlameGraph scripts if not already present:

```bash
git clone https://github.com/brendangregg/FlameGraph scripts/FlameGraph
```

Generate the flame graph colored by memory/cache miss events:

```bash
./scripts/FlameGraph/stackcollapse-perf.pl out.perf > out.folded
./scripts/FlameGraph/flamegraph.pl \
  --title "L1 Cache Misses: Baseline vs O(1) Engine" \
  --countname "misses" \
  --colors mem \
  out.folded > side_by_side.svg
```

### Step 4 — View the result

On WSL2, open directly in your Windows browser:

```bash
explorer.exe side_by_side.svg
```

The baseline STL algorithms (`std::sort`, `_M_realloc_insert`, Red-Black tree traversal) will dominate the visible flame graph space. The NANOMATCH O(1) engine's stack frames will be nearly invisible by comparison — operating well within the L1 cache.

---

## Performance Results

Benchmarked with Google Benchmark (100 repetitions, CPU pinned to Core 7):

| Benchmark | mean | median | stddev | cv | p50 | p90 | p99 |
|---|---|---|---|---|---|---|---|
| `OrderBookFixture/OptimizedLatency` | 686 ns | 649 ns | 248 ns | 36.08% | 639 ns | 1,021 ns | 1,289 ns |
| `MapBasedFixture/MapLatency` | 77,118 ns | 67,704 ns | 26,863 ns | 34.83% | 67,673 ns | 116,992 ns | 136,901 ns |
| `BaselineFixture/BaselineLatency` | 353,555 ns | 344,463 ns | 32,543 ns | 9.20% | 344,388 ns | 387,591 ns | 489,067 ns |

> The optimized engine (bitboard + memory pool) is **515× faster** than the baseline (STL vector + sort) and **112× faster** than the map-based implementation at mean latency.

---

## Hardware Environment

| Component | Specification |
|---|---|
| **CPU** | 8 Cores @ 2112.01 MHz |
| **L1 Cache** | 32 KiB Data × 4, 32 KiB Instruction × 4 |
| **L2 Cache** | 256 KiB Unified × 4 |
| **L3 Cache** | 6144 KiB Unified × 1 |
| **OS** | Linux WSL2 on Windows Hypervisor |
| **Compiler** | GCC with flags `-O3 -march=native` |

---

*Built for the quantitative systems engineering track. Architecture decisions validated against NASDAQ TotalView-ITCH specifications.*