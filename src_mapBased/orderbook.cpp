#include "orderbook.h"
#include <iostream>
#include <algorithm>

namespace map_queue {

    void MapOrderBook::add_order(Order order) {
        if (order.is_buy) {
            while (!asks.empty() && order.quantity > 0 && order.price >= asks.begin()->first) {
                auto best_ask_it = asks.begin();
                PriceLevel& best_ask_level = best_ask_it->second;

                Order& current_ask = best_ask_level.orders.front();

                uint64_t matched_qty = std::min(order.quantity, current_ask.quantity);
                
                order.quantity -= matched_qty;
                current_ask.quantity -= matched_qty;
                best_ask_level.total_volume -= matched_qty;

                if (current_ask.quantity == 0) {
                    order_lookup.erase(current_ask.order_id);
                    best_ask_level.orders.pop_front();
                }

                if (best_ask_level.orders.empty()) {
                    asks.erase(best_ask_it);
                }
            }
        } else {
            while (!bids.empty() && order.quantity > 0 && order.price <= bids.begin()->first) {
                auto best_bid_it = bids.begin();
                PriceLevel& best_bid_level = best_bid_it->second;

                Order& current_bid = best_bid_level.orders.front();

                uint64_t matched_qty = std::min(order.quantity, current_bid.quantity);
                
                order.quantity -= matched_qty;
                current_bid.quantity -= matched_qty;
                best_bid_level.total_volume -= matched_qty;

                if (current_bid.quantity == 0) {
                    order_lookup.erase(current_bid.order_id);
                    best_bid_level.orders.pop_front();
                }

                if (best_bid_level.orders.empty()) {
                    bids.erase(best_bid_it);
                }
            }
        }

        if (order.quantity > 0) {
            PriceLevel* level_ptr = nullptr;
            std::list<Order>::iterator order_it;

            if (order.is_buy) {
                bids[order.price].price = order.price; 
                level_ptr = &bids[order.price];
                order_it = level_ptr->add_order(order);
            } else {
                asks[order.price].price = order.price;
                level_ptr = &asks[order.price];
                order_it = level_ptr->add_order(order);
            }

            order_lookup[order.order_id] = {level_ptr, order_it};
        }
    }

    void MapOrderBook::cancel_order(uint64_t order_id) {
        // 1. O(1) Hash Map Lookup
        auto it = order_lookup.find(order_id);
        if (it == order_lookup.end()) return; 

        PriceLevel* level = it->second.first;
        auto list_iterator = it->second.second;

        bool is_buy = list_iterator->is_buy;
        uint64_t price = level->price;

        level->total_volume -= list_iterator->quantity;
        level->orders.erase(list_iterator); 
        
        if (level->orders.empty()) {
            if (is_buy) bids.erase(price);
            else asks.erase(price);
        }

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