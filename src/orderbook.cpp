#include "orderbook.h"
#include <iostream>
#include <algorithm>

void OrderBook::add_to_list(PriceLevel& level, int32_t order_idx) {
    Order& order = pool.get(order_idx);
    order.next_index = -1;
    order.prev_index = level.tail_order_index;

    if (level.head_order_index == -1) {
        level.head_order_index = order_idx;
        level.tail_order_index = order_idx;
    } else {
        pool.get(level.tail_order_index).next_index = order_idx;
        level.tail_order_index = order_idx;
    }
    level.total_volume += order.quantity;
}

void OrderBook::remove_from_list(PriceLevel& level, int32_t order_idx) {
    Order& order = pool.get(order_idx);

    if (order.prev_index != -1) {
        pool.get(order.prev_index).next_index = order.next_index;
    } else {
        level.head_order_index = order.next_index; 
    }

    if (order.next_index != -1) {
        pool.get(order.next_index).prev_index = order.prev_index;
    } else {
        level.tail_order_index = order.prev_index; 
    }

    level.total_volume -= order.quantity;
    order.next_index = -1;
    order.prev_index = -1;
}

void OrderBook::update_best_bid_downwards() {
    uint32_t block_idx = best_bid / 64;
    
    uint64_t mask = (1ULL << (best_bid % 64)) - 1; 
    uint64_t masked_block = bid_bitboard[block_idx] & mask;

    if (masked_block != 0) {
        best_bid = (block_idx * 64) + (63 - __builtin_clzll(masked_block));
        return;
    }

    while (block_idx > 0) {
        block_idx--;
        if (bid_bitboard[block_idx] != 0) {
            best_bid = (block_idx * 64) + (63 - __builtin_clzll(bid_bitboard[block_idx]));
            return;
        }
    }
    best_bid = 0;
}

void OrderBook::update_best_ask_upwards() {
    uint32_t block_idx = best_ask / 64;
    
    uint32_t shift_amount = (best_ask % 64) + 1;
    uint64_t masked_block = (shift_amount == 64) ? 0 : (ask_bitboard[block_idx] >> shift_amount);

    if (masked_block != 0) {
        best_ask += 1 + __builtin_ctzll(masked_block);
        return;
    }

    uint32_t max_blocks = ask_bitboard.size();
    while (++block_idx < max_blocks) {
        if (ask_bitboard[block_idx] != 0) {
            best_ask = (block_idx * 64) + __builtin_ctzll(ask_bitboard[block_idx]);
            return;
        }
    }
    best_ask = MAX_PRICE; 
}

void OrderBook::add_order(const Order& incoming_order) {
    int32_t idx = pool.allocate();
    Order& order = pool.get(idx);
    order = incoming_order; 
    order.next_index = -1;
    order.prev_index = -1;

    order_map[incoming_order.order_id] = idx;

    if (order.type == OrderType::MARKET) { 
        if (order.side == Side::BUY) execute_market_buy(idx);
        else execute_market_sell(idx);
    } else { 
        if (order.side == Side::BUY) execute_limit_buy(idx);
        else execute_limit_sell(idx);
    }
}

void OrderBook::cancel_order(uint64_t order_id) {
    if (order_id >= order_map.size() || order_map[order_id] == -1) return;

    int32_t idx = order_map[order_id];
    Order& order = pool.get(idx);

    if (order.side == Side::BUY) {
        remove_from_list(bids[order.price], idx);
    
        if (bids[order.price].head_order_index == -1) {
            clear_bid_active(order.price); 
            
            if (order.price == best_bid) {
                update_best_bid_downwards();
            }
        }
    } else {
        remove_from_list(asks[order.price], idx);
        
        if (asks[order.price].head_order_index == -1) {
            clear_ask_active(order.price);
            
            if (order.price == best_ask) {
                update_best_ask_upwards();
            }
        }
    }

    order_map[order_id] = -1;
    pool.deallocate(idx);
}

void OrderBook::execute_limit_buy(int32_t incoming_idx) {
    Order& buy_order = pool.get(incoming_idx);

    while (buy_order.quantity > 0 && best_ask <= MAX_PRICE) {
        if (buy_order.price < best_ask) break;

        PriceLevel& ask_level = asks[best_ask];
        int32_t current_ask_idx = ask_level.head_order_index;
        
        while (current_ask_idx != -1 && buy_order.quantity > 0) {
            Order& resting_ask = pool.get(current_ask_idx);
            uint32_t trade_qty = std::min(buy_order.quantity, resting_ask.quantity);

            logger.log_trade(buy_order.order_id, resting_ask.order_id, best_ask, trade_qty);

            
            buy_order.quantity -= trade_qty;
            resting_ask.quantity -= trade_qty;
            ask_level.total_volume -= trade_qty;

            int32_t next_idx = resting_ask.next_index;

            if (resting_ask.quantity == 0) {
                remove_from_list(ask_level, current_ask_idx);
                order_map[resting_ask.order_id] = -1;
                pool.deallocate(current_ask_idx);
            }
            current_ask_idx = next_idx;
        }

        // Walk the book up if level is emptied
        if (ask_level.head_order_index == -1) {
            clear_ask_active(best_ask);   
            update_best_ask_upwards();    
        }
    }

    if (buy_order.quantity > 0) {
        add_to_list(bids[buy_order.price], incoming_idx);
        order_map[buy_order.order_id] = incoming_idx;
        
        set_bid_active(buy_order.price); 
    } else {
        pool.deallocate(incoming_idx);
    }
}

void OrderBook::execute_limit_sell(int32_t incoming_idx) {
    Order& sell_order = pool.get(incoming_idx);

    while (sell_order.quantity > 0 && best_bid > 0) {
        if (sell_order.price > best_bid) break; 

        PriceLevel& bid_level = bids[best_bid];
        int32_t current_bid_idx = bid_level.head_order_index;

        
        
        while (current_bid_idx != -1 && sell_order.quantity > 0) {
            Order& resting_bid = pool.get(current_bid_idx);
            uint32_t trade_qty = std::min(sell_order.quantity, resting_bid.quantity);

            logger.log_trade(resting_bid.order_id, sell_order.order_id, best_bid, trade_qty);

            sell_order.quantity -= trade_qty;
            resting_bid.quantity -= trade_qty;
            bid_level.total_volume -= trade_qty;

            int32_t next_idx = resting_bid.next_index;

            if (resting_bid.quantity == 0) {
                remove_from_list(bid_level, current_bid_idx);
                order_map[resting_bid.order_id] = -1;
                pool.deallocate(current_bid_idx);
            }
            current_bid_idx = next_idx;
        }

        if (bid_level.head_order_index == -1) {
            clear_bid_active(best_bid);     
            update_best_bid_downwards();    
        }
    }

    if (sell_order.quantity > 0) {
        add_to_list(asks[sell_order.price], incoming_idx);
        order_map[sell_order.order_id] = incoming_idx;
        
        set_ask_active(sell_order.price); 
    } else {
        pool.deallocate(incoming_idx);
    }
}

void OrderBook::execute_market_buy(int32_t incoming_idx) {
    Order& buy_order = pool.get(incoming_idx);

    while (buy_order.quantity > 0 && best_ask <= MAX_PRICE) {
        PriceLevel& ask_level = asks[best_ask];
        int32_t current_ask_idx = ask_level.head_order_index;
        
        while (current_ask_idx != -1 && buy_order.quantity > 0) {
            Order& resting_ask = pool.get(current_ask_idx);
            uint32_t trade_qty = std::min(buy_order.quantity, resting_ask.quantity);
            logger.log_trade(buy_order.order_id, resting_ask.order_id, best_ask, trade_qty);

            buy_order.quantity -= trade_qty;
            resting_ask.quantity -= trade_qty;
            ask_level.total_volume -= trade_qty;

            int32_t next_idx = resting_ask.next_index;

            if (resting_ask.quantity == 0) {
                remove_from_list(ask_level, current_ask_idx);
                order_map[resting_ask.order_id] = -1;
                pool.deallocate(current_ask_idx);
            }
            current_ask_idx = next_idx;
        }

        if (ask_level.head_order_index == -1) {
            clear_ask_active(best_ask);    // 1. Flip the bit for this empty ask to 0
            update_best_ask_upwards();     // 2. Hardware-scan instantly to the next lowest ask
        }
    }

    order_map[buy_order.order_id] = -1;
    pool.deallocate(incoming_idx);
}

void OrderBook::execute_market_sell(int32_t incoming_idx) {
    Order& sell_order = pool.get(incoming_idx);

    while (sell_order.quantity > 0 && best_bid > 0) {
        PriceLevel& bid_level = bids[best_bid];
        int32_t current_bid_idx = bid_level.head_order_index;
        
        while (current_bid_idx != -1 && sell_order.quantity > 0) {
            Order& resting_bid = pool.get(current_bid_idx);
            uint32_t trade_qty = std::min(sell_order.quantity, resting_bid.quantity);
            logger.log_trade(resting_bid.order_id, sell_order.order_id, best_bid, trade_qty);
            
            sell_order.quantity -= trade_qty;
            resting_bid.quantity -= trade_qty;
            bid_level.total_volume -= trade_qty;

            int32_t next_idx = resting_bid.next_index;

            if (resting_bid.quantity == 0) {
                remove_from_list(bid_level, current_bid_idx);
                order_map[resting_bid.order_id] = -1;
                pool.deallocate(current_bid_idx);
            }
            current_bid_idx = next_idx;
        }

        if (bid_level.head_order_index == -1) {
            clear_bid_active(best_bid);     
            update_best_bid_downwards();    
        }
    }

    order_map[sell_order.order_id] = -1;
    pool.deallocate(incoming_idx);
}

void OrderBook::print_book() {
    std::cout << "\n=== ULTRA-LOW-LATENCY LADDER ===\n";
    std::cout << "--- ASKS (Sellers) ---\n";
    
    int printed = 0;
    for (uint32_t p = best_ask; p < MAX_PRICE && printed < 5; ++p) {
        if (asks[p].head_order_index != -1) {
            std::cout << "Price: " << p << " | Volume: " << asks[p].total_volume << "\n";
            printed++;
        }
    }

    std::cout << "-----------------------\n";
    std::cout << "--- BIDS (Buyers)  ---\n";
    
    printed = 0;
    for (uint32_t p = best_bid; p > 0 && printed < 5; --p) {
        if (bids[p].head_order_index != -1) {
            std::cout << "Price: " << p << " | Volume: " << bids[p].total_volume << "\n";
            printed++;
        }
    }
    std::cout << "================================\n\n";
}