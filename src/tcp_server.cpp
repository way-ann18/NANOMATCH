#include "tcp_server.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <chrono>

TCPServer::TCPServer(int listen_port, OrderBook& order_book) 
    : port(listen_port), book(order_book) {
    
    // 1. Create the socket file descriptor
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        std::cerr << "[ERROR] Socket creation failed.\n";
        exit(EXIT_FAILURE);
    }

    // 2. Bind the socket to the port
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        std::cerr << "[ERROR] Bind failed. Port might be in use.\n";
        exit(EXIT_FAILURE);
    }
}

TCPServer::~TCPServer() {
    close(server_fd);
}

void TCPServer::start_listening() {
    // 3. Start listening for incoming connections
    if (listen(server_fd, 3) < 0) {
        std::cerr << "[ERROR] Listen failed.\n";
        return;
    }

    std::cout << "[SYSTEM] Nanomatch Live Exchange online.\n";
    std::cout << "[SYSTEM] Listening for algo bots on Port " << port << "...\n";

    while (true) {
        struct sockaddr_in client_address;
        socklen_t addrlen = sizeof(client_address);
        
        // Block and wait until a trader connects
        int client_socket = accept(server_fd, (struct sockaddr *)&client_address, &addrlen);
        if (client_socket < 0) {
            std::cerr << "[ERROR] Accept failed.\n";
            continue;
        }

        std::cout << "\n[NETWORK] Trader connected! Receiving data stream...\n";

        BinaryOrderPacket packet;
        int orders_received = 0;

        // Keep reading 18-byte chunks until the client disconnects
        while (read(client_socket, &packet, sizeof(BinaryOrderPacket)) == sizeof(BinaryOrderPacket)) {
            Order order;
            order.order_id = packet.order_id;
            order.price = packet.price;
            order.quantity = packet.quantity;
            order.side = static_cast<Side>(packet.side);
            order.type = static_cast<OrderType>(packet.type);
            
            // Get current timestamp in nanoseconds since epoch
            order.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            // Fire into the matching engine
            book.add_order(order);
            orders_received++;
        }

        std::cout << "[NETWORK] Trader disconnected. Processed " << orders_received << " live orders.\n";
        book.print_book();
        
        close(client_socket);
    }
}