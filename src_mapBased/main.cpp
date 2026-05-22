#include <iostream>
#include "orderbook.h" 

using namespace map_queue;

int main() {
    MapOrderBook ob;

    std::cout << "=========================================\n";
    std::cout << "  SCENARIO 1: BUILDING INITIAL LIQUIDITY \n";
    std::cout << "=========================================\n";
    // Adding bids (Buyers)
    ob.add_order({1, true, 98, 100});   // Bid 100 shares @ $98
    ob.add_order({2, true, 99, 200});   // Bid 200 shares @ $99
    
    // Adding asks (Sellers)
    ob.add_order({3, false, 102, 100}); // Ask 100 shares @ $102
    ob.add_order({4, false, 101, 150}); // Ask 150 shares @ $101

    // Expected Spread: 99 (Bid) to 101 (Ask)
    ob.print_top_of_book();


    std::cout << "=========================================\n";
    std::cout << "  SCENARIO 2: AGGRESSIVE PARTIAL FILL    \n";
    std::cout << "=========================================\n";
    std::cout << "-> A seller arrives wanting to dump 50 shares at $99.\n";
    std::cout << "-> This should match with Order 2, reducing the $99 Bid level to 150 shares.\n";
    
    ob.add_order({5, false, 99, 50}); 
    
    ob.print_top_of_book();


    std::cout << "=========================================\n";
    std::cout << "  SCENARIO 3: CLEARING A PRICE LEVEL     \n";
    std::cout << "=========================================\n";
    std::cout << "-> A buyer arrives wanting 200 shares, willing to pay up to $102.\n";
    std::cout << "-> This should completely wipe out the $101 Ask level (150 shares).\n";
    std::cout << "-> The remaining 50 shares should trade against the $102 Ask level.\n";
    std::cout << "-> The new Best Ask should become $102 with 50 shares remaining.\n";

    ob.add_order({6, true, 102, 200});

    ob.print_top_of_book();


    std::cout << "=========================================\n";
    std::cout << "  SCENARIO 4: RESTING LIMIT ORDER        \n";
    std::cout << "=========================================\n";
    std::cout << "-> A buyer arrives wanting 500 shares at $105.\n";
    std::cout << "-> It eats the remaining 50 shares at $102.\n";
    std::cout << "-> The remaining 450 shares rest on the book as a new Best Bid at $105.\n";

    ob.add_order({7, true, 105, 500});

    ob.print_top_of_book();

    return 0;
}