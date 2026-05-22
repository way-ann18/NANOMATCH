#include "csv_reader.h"
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// --- THE ULTRA-FAST ASCII PARSER ---
// This completely bypasses std::stoi and std::string allocations.
// It reads raw bytes and calculates the integer mathematically.
inline uint64_t parse_uint(const char*& cursor) {
    uint64_t result = 0;
    while (*cursor >= '0' && *cursor <= '9') {
        result = (result * 10) + (*cursor - '0');
        cursor++;
    }
    // Skip the delimiter (comma, newline, or carriage return)
    while (*cursor == ',' || *cursor == '\n' || *cursor == '\r') {
        cursor++;
    }
    return result;
}

void CSVReader::load_orders(const std::string& filepath, OrderBook& book) {
    std::cout << "[SYSTEM] Engaging zero-copy mmap for " << filepath << "...\n";

    // 1. Open the file directly via the Linux Kernel
    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "[ERROR] Could not open file: " << filepath << "\n";
        return;
    }

    // 2. Get the exact size of the file in bytes
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        std::cerr << "[ERROR] Could not get file size.\n";
        close(fd);
        return;
    }
    size_t length = sb.st_size;

    // 3. Map the file directly into RAM (Zero-Copy)
    const char* data = static_cast<const char*>(mmap(nullptr, length, PROT_READ, MAP_PRIVATE, fd, 0));
    if (data == MAP_FAILED) {
        std::cerr << "[ERROR] mmap failed to allocate memory.\n";
        close(fd);
        return;
    }

    // 4. Sweep the pointers across the mapped memory
    const char* cursor = data;
    const char* end = data + length;
    int count = 0;

    while (cursor < end && *cursor != '\0') {
        Order order;
        
        // Parse directly into the struct
        order.order_id = parse_uint(cursor);
        if (cursor >= end) break; // Safety check for EOF
        
        order.timestamp = parse_uint(cursor);
        order.price = parse_uint(cursor);
        order.quantity = parse_uint(cursor);
        order.side = static_cast<Side>(parse_uint(cursor));
        order.type = static_cast<OrderType>(parse_uint(cursor));

        // Blast into the MemoryPool
        book.add_order(order);
        count++;
    }

    // 5. Clean up Linux system resources
    munmap((void*)data, length);
    close(fd);

    std::cout << "[SYSTEM] Successfully mmap-parsed " << count << " orders.\n";
}