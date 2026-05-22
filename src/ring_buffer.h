#pragma once
#include <atomic>
#include <vector>
#include <cstddef>

template <typename T, size_t Size>
class SPSCRingBuffer {
    // Hardware optimization: Force the size to be a power of 2 at compile time.
    // This allows us to use bitwise AND (&) instead of the slow modulo (%) operator.
    static_assert((Size & (Size - 1)) == 0, "RingBuffer size MUST be a power of 2!");

private:
    std::vector<T> buffer;

    // alignas(64) ensures these two atomic counters sit on completely different 
    // L1 Cache Lines in the CPU. If they shared a cache line, the Main Thread 
    // and Background Thread would lock each other out at the hardware level!
    alignas(64) std::atomic<size_t> write_index{0}; // Modified only by Producer
    alignas(64) std::atomic<size_t> read_index{0};  // Modified only by Consumer

public:
    SPSCRingBuffer() : buffer(Size) {}

    // ------------------------------------------------------------------------
    // PRODUCER: Push data into the buffer (O(1), Lock-Free)
    // ------------------------------------------------------------------------
    bool push(const T& item) {
        size_t current_write = write_index.load(std::memory_order_relaxed);
        size_t next_write = (current_write + 1) & (Size - 1);

        // Check if the buffer is full (write pointer caught up to read pointer)
        if (next_write == read_index.load(std::memory_order_acquire)) {
            return false; // Cannot push, buffer is full
        }

        buffer[current_write] = item;
        
        // memory_order_release guarantees the data is physically written to RAM 
        // BEFORE the read thread is allowed to see the updated write_index.
        write_index.store(next_write, std::memory_order_release);
        return true;
    }

    // ------------------------------------------------------------------------
    // CONSUMER: Pop data from the buffer (O(1), Lock-Free)
    // ------------------------------------------------------------------------
    bool pop(T& item) {
        size_t current_read = read_index.load(std::memory_order_relaxed);

        // Check if the buffer is empty
        if (current_read == write_index.load(std::memory_order_acquire)) {
            return false; // Nothing to read
        }

        item = buffer[current_read];
        
        size_t next_read = (current_read + 1) & (Size - 1);
        read_index.store(next_read, std::memory_order_release);
        return true;
    }
};