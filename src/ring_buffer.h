#pragma once
#include <atomic>
#include <vector>
#include <cstddef>

template <typename T, size_t Size>
class SPSCRingBuffer {
    static_assert((Size & (Size - 1)) == 0, "RingBuffer size MUST be a power of 2!");

private:
    std::vector<T> buffer;

    alignas(64) std::atomic<size_t> write_index{0}; 
    alignas(64) std::atomic<size_t> read_index{0};  

public:
    SPSCRingBuffer() : buffer(Size) {}

    bool push(const T& item) {
        size_t current_write = write_index.load(std::memory_order_relaxed);
        size_t next_write = (current_write + 1) & (Size - 1);

        if (next_write == read_index.load(std::memory_order_acquire)) {
            return false; // Cannot push, buffer is full
        }

        buffer[current_write] = item;
        
        write_index.store(next_write, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        size_t current_read = read_index.load(std::memory_order_relaxed);

        if (current_read == write_index.load(std::memory_order_acquire)) {
            return false; 
        }

        item = buffer[current_read];
        
        size_t next_read = (current_read + 1) & (Size - 1);
        read_index.store(next_read, std::memory_order_release);
        return true;
    }
};