cmake_minimum_required(VERSION 3.15)
project(NANOMATCH)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

add_compile_options(-O3 -march=native)

add_executable(NANOMATCH
    src/main.cpp
    src/orderbook.cpp
    src/csv_reader.cpp
    src/tcp_server.cpp
    src/logger.cpp
)

target_include_directories(NANOMATCH PRIVATE src)
find_package(Threads REQUIRED)
target_link_libraries(NANOMATCH PRIVATE Threads::Threads)

include(FetchContent)
FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.3
)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark testing" FORCE)

FetchContent_MakeAvailable(benchmark)

# --- 1. Baseline Library ---
add_library(baseline_lib STATIC src_baseline/orderbook.cpp)
target_include_directories(baseline_lib PUBLIC src_baseline)

# --- 2. NEW: Map & Queue Based Library ---
add_library(map_lib STATIC src_mapBased/orderbook.cpp)
target_include_directories(map_lib PUBLIC src_mapBased)

add_executable(nanomatch_bench benchmark.cpp src/orderbook.cpp src/logger.cpp)
target_include_directories(nanomatch_bench PRIVATE src)

# Add these to CMakeLists.txt if you want separate runnables
add_executable(RUN_BASELINE src_baseline/main.cpp src_baseline/orderbook.cpp)
target_link_libraries(RUN_BASELINE PRIVATE baseline_lib Threads::Threads)

add_executable(RUN_MAP src_mapBased/main.cpp src_mapBased/orderbook.cpp)
target_link_libraries(RUN_MAP PRIVATE map_lib Threads::Threads)
# --- 3. NEW: V1 Library ---

# --- 3. Link all 3 libraries to the benchmark executable ---
target_link_libraries(nanomatch_bench 
    PRIVATE 
        baseline_lib 
        map_lib 
        benchmark::benchmark 
        Threads::Threads
)