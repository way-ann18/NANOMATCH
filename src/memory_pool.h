#pragma once
#include "order.h"
#include <vector>
#include <stdexcept>
#include <iostream>

class MemoryPool {
private:
    std::vector<Order> pool;        // The giant pre-allocated contiguous block of memory
    std::vector<int32_t> free_list; // Stack of available indices

public:
    MemoryPool(size_t capacity = 1000000) {
        std::cout << "[SYSTEM] Pre-allocating memory for " << capacity << " orders...\n";
        
        pool.resize(capacity);
        
        free_list.reserve(capacity);
        for (int32_t i = capacity - 1; i >= 0; --i) {
            free_list.push_back(i);
        }
    }

    int32_t allocate() {
        if (free_list.empty()) {
            throw std::runtime_error("CRITICAL: Memory Pool Exhausted!");
        }
        int32_t index = free_list.back();
        free_list.pop_back();
        return index;
    }

    void deallocate(int32_t index) {
        free_list.push_back(index);
    }

    // Get a reference to the order at a specific index
    Order& get(int32_t index) {
        return pool[index];
    }
};