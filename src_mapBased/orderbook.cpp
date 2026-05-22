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

                if (current_bid.quantity == 0) best_bid_level.orders.pop();
                if (current_ask.quantity == 0) best_ask_level.orders.pop();

                if (best_bid_level.orders.empty()) bids.erase(best_bid_it);
                if (best_ask_level.orders.empty()) asks.erase(best_ask_it);
            } else {
                break;
            }
        }
    }

    void MapOrderBook::add_order(const Order& order) {
        if (order.is_buy) {
            bids[order.price].price = order.price; 
            bids[order.price].add_order(order);
        } else {
            asks[order.price].price = order.price;
            asks[order.price].add_order(order);
        }
        match();
    }

    void MapOrderBook::cancel_order(uint64_t order_id) {
        auto cancel_in_map = [&](auto& book_map) {
            for (auto& [price, level] : book_map) {
                bool found = false;
                size_t size = level.orders.size();
                std::queue<Order> temp_queue;

                for (size_t i = 0; i < size; ++i) {
                    Order order = level.orders.front();
                    level.orders.pop();
                    if (order.order_id == order_id) {
                        level.total_volume -= order.quantity;
                        found = true;
                    } else {
                        temp_queue.push(order);
                    }
                }
                level.orders = std::move(temp_queue);
                if (found) {
                    if (level.orders.empty()) book_map.erase(price);
                    return true;
                }
            }
            return false;
        };

        if (!cancel_in_map(bids)) {
            cancel_in_map(asks);
        }
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