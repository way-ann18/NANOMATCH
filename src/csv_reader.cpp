#include "csv_reader.h"
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

inline uint64_t parse_uint(const char*& cursor) {
    uint64_t result = 0;
    while (*cursor >= '0' && *cursor <= '9') {
        result = (result * 10) + (*cursor - '0');
        cursor++;
    }
    while (*cursor == ',' || *cursor == '\n' || *cursor == '\r') {
        cursor++;
    }
    return result;
}

void CSVReader::load_orders(const std::string& filepath, OrderBook& book) {
    std::cout << "[SYSTEM] Engaging zero-copy mmap for " << filepath << "...\n";

    int fd = open(filepath.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "[ERROR] Could not open file: " << filepath << "\n";
        return;
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        std::cerr << "[ERROR] Could not get file size.\n";
        close(fd);
        return;
    }
    size_t length = sb.st_size;

    const char* data = static_cast<const char*>(mmap(nullptr, length, PROT_READ, MAP_PRIVATE, fd, 0));
    if (data == MAP_FAILED) {
        std::cerr << "[ERROR] mmap failed to allocate memory.\n";
        close(fd);
        return;
    }

    const char* cursor = data;
    const char* end = data + length;
    int count = 0;

    while (cursor < end && *cursor != '\0') {
        Order order;
        
        order.order_id = parse_uint(cursor);
        if (cursor >= end) break; // Safety check for EOF
        
        order.timestamp = parse_uint(cursor);
        order.price = parse_uint(cursor);
        order.quantity = parse_uint(cursor);
        order.side = static_cast<Side>(parse_uint(cursor));
        order.type = static_cast<OrderType>(parse_uint(cursor));

        book.add_order(order);
        count++;
    }

    munmap((void*)data, length);
    close(fd);

    std::cout << "[SYSTEM] Successfully mmap-parsed " << count << " orders.\n";
}