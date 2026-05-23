#include "orderbook.h"
#include <iostream>
#include <algorithm>

namespace map_queue {

    void MapOrderBook::match() {
        while (!bids.empty() && !asks.empty()) {
            auto best_bid_it = bids.begin();
            auto best_ask_it = asks.begin();

            if (best_bid_it->first >= best_ask_it->first) {
                PriceLevel& best_bid_level = best_bid_it->second;
                PriceLevel& best_ask_level = best_ask_it->second;

                Order& current_bid = best_bid_level.orders.front();
                Order& current_ask = best_ask_level.orders.front();

                uint64_t matched_qty = std::min(current_bid.quantity, current_ask.quantity);
                
                current_bid.quantity -= matched_qty;
                current_ask.quantity -= matched_qty;

                best_bid_level.total_volume -= matched_qty;
                best_ask_level.total_volume -= matched_qty;

                // CHANGED: Use pop_front() for std::list and clean up the lookup map
                if (current_bid.quantity == 0) {
                    order_lookup.erase(current_bid.order_id); // Clean up map
                    best_bid_level.orders.pop_front();
                }
                if (current_ask.quantity == 0) {
                    order_lookup.erase(current_ask.order_id); // Clean up map
                    best_ask_level.orders.pop_front();
                }

                if (best_bid_level.orders.empty()) bids.erase(best_bid_it);
                if (best_ask_level.orders.empty()) asks.erase(best_ask_it);
            } else {
                break;
            }
        }
    }

    void MapOrderBook::add_order(const Order& order) {
        PriceLevel* level_ptr = nullptr;
        std::list<Order>::iterator order_it;

        // CHANGED: Capture the pointer to the level and the iterator to the order
        if (order.is_buy) {
            bids[order.price].price = order.price; 
            level_ptr = &bids[order.price];
            order_it = level_ptr->add_order(order);
        } else {
            asks[order.price].price = order.price;
            level_ptr = &asks[order.price];
            order_it = level_ptr->add_order(order);
        }

        // NEW: Store it in the hash map for O(1) lookups later
        order_lookup[order.order_id] = {level_ptr, order_it};

        match();
    }

    // ENTIRELY REWRITTEN: Now executes in true O(1) time
    void MapOrderBook::cancel_order(uint64_t order_id) {
        // 1. O(1) Hash Map Lookup
        auto it = order_lookup.find(order_id);
        if (it == order_lookup.end()) return; // Order not found, exit instantly

        PriceLevel* level = it->second.first;
        auto list_iterator = it->second.second;

        bool is_buy = list_iterator->is_buy;
        uint64_t price = level->price;

        // 2. O(1) Math & List Deletion
        level->total_volume -= list_iterator->quantity;
        level->orders.erase(list_iterator); 
        
        // 3. Clean up the Price Level if it's completely empty
        if (level->orders.empty()) {
            if (is_buy) bids.erase(price);
            else asks.erase(price);
        }

        // 4. Erase from the lookup table
        order_lookup.erase(it);
    }

    void MapOrderBook::print_top_of_book() const {
        std::cout << "\n--- L2 MARKET DATA (Top of Book) ---\n";
        
        if (!asks.empty()) {
            const PriceLevel& best_ask = asks.begin()->second;
            std::cout << "ASK: " << best_ask.total_volume << " shares @ $" 
                      << best_ask.price << " (" << best_ask.orders.size() << " orders)\n";
        } else {
            std::cout << "ASK: NONE\n";
        }
        
        if (!bids.empty()) {
            const PriceLevel& best_bid = bids.begin()->second;
            std::cout << "BID: " << best_bid.total_volume << " shares @ $" 
                      << best_bid.price << " (" << best_bid.orders.size() << " orders)\n";
        } else {
            std::cout << "BID: NONE\n";
        }
        std::cout << "------------------------------------\n\n";
    }

}