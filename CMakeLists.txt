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
# Tell CMake to link the system thread library (required for our logger)
find_package(Threads REQUIRED)
target_link_libraries(NANOMATCH PRIVATE Threads::Threads)

# --- 2. FETCH GOOGLE BENCHMARK ---
include(FetchContent)
FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG v1.8.3
)

# Tell Google Benchmark NOT to download GoogleTest
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark testing" FORCE)

FetchContent_MakeAvailable(benchmark)

# --- 3. BASELINE LIBRARY ---
# Compile the baseline as a library, explicitly skipping its main.cpp
add_library(baseline_lib STATIC src_baseline/orderbook.cpp)
target_include_directories(baseline_lib PUBLIC src_baseline)

# --- 4. THE BENCHMARK EXECUTABLE ---
# We create a completely separate binary just for statistical testing
add_executable(nanomatch_bench src/benchmark.cpp src/orderbook.cpp src/logger.cpp)
target_include_directories(nanomatch_bench PRIVATE src)

target_link_libraries(nanomatch_bench PRIVATE baseline_lib benchmark::benchmark Threads::Threads)