#include "orderbook.h"
#include<iostream>
#include<algorithm>

namespace baseline{
    bool OrderBook::compareBids(const Order& a, const Order& b){
        if(a.price!=b.price){
            return a.price>b.price;
        }
        return a.timestamp<b.timestamp;
    }

    bool OrderBook::compareAsks(const Order& a, const Order& b){
        if(a.price!=b.price){
            return a.price<b.price;
        }
        return a.timestamp<b.timestamp;
    }

    void OrderBook::add_order(Order order){
        if(order.side==Side::BUY){
            match_buy(order);
        }
        else{
            match_sell(order);
        }
    }

    void OrderBook::match_buy(Order& buy_order){
        while(buy_order.quantity>0 && !asks.empty()){
            Order& best_ask=asks.front();

            if(buy_order.type==OrderType::LIMIT && buy_order.price<best_ask.price){
                break;
            }

            uint32_t trade_quantity=std::min(buy_order.quantity, best_ask.quantity);
            uint32_t execution_price=best_ask.price;

            // std::cout<<"[TRADE EXECUTION] Buyer "<<buy_order.order_id<<" bought "<<trade_quantity<<" units from Seller "<<best_ask.order_id<<" at price "<<execution_price<<"\n";

            buy_order.quantity-=trade_quantity;
            best_ask.quantity-=trade_quantity;

            if(best_ask.quantity==0){
                asks.erase(asks.begin());
            }
        }

        if(buy_order.quantity>0){
            if(buy_order.type==OrderType::LIMIT){
                bids.push_back(buy_order);
                std::sort(bids.begin(), bids.end(), compareBids);
            }
            else if(buy_order.type==OrderType::MARKET){
                // std::cout<<"[MARKET CANCELLED] Buyer "<<buy_order.order_id<<" cancelled remaining "<<buy_order.quantity<<" units (no liquidity left).\n";
            }
        }
    }

    void OrderBook::match_sell(Order& sell_order){
        while(sell_order.quantity>0 && !bids.empty()){
            Order& best_bid=bids.front();
            if(sell_order.type==OrderType::LIMIT && sell_order.price>best_bid.price){
                break;
            }

            uint32_t trade_quantity=std::min(sell_order.quantity, best_bid.quantity);
            uint32_t execution_price=best_bid.price;

            // std::cout<<"[TRADE EXECUTION] Seller "<<sell_order.order_id<<" sold "<<trade_quantity<<" unit to Buyer "<<best_bid.order_id<<" at price "<<execution_price<<"\n";
            sell_order.quantity-=trade_quantity;
            best_bid.quantity-=trade_quantity;

            if(best_bid.quantity==0){
                bids.erase(bids.begin());
            }
        }

        if(sell_order.quantity>0){
            if(sell_order.type==OrderType::LIMIT){
                asks.push_back(sell_order);
                std::sort(asks.begin(), asks.end(), compareAsks);
            }
            else if(sell_order.type==OrderType::MARKET){
                // std::cout<<"[MARKET CANCELLED] Seller"<<sell_order.order_id<<" cancelled remaining "<<sell_order.quantity<<" units (no liquidity left).\n";
            }
        }
    }
    void OrderBook::cancel_order(uint64_t target_order_id){
        auto bid_it=std::find_if(bids.begin(), bids.end(), [target_order_id](const Order& o) {return o.order_id==target_order_id;});

        if(bid_it!=bids.end()){
            // std::cout<<"[CANCEL] Buyer canceled order ID "<<target_order_id<<"\n";
            bids.erase(bid_it);
            return;
        }

        auto ask_it=std::find_if(asks.begin(), asks.end(), [target_order_id](const Order& o) {return o.order_id==target_order_id;});
        
        if(ask_it!=asks.end()){
            // std::cout<<"[CANCEL] Seller canceled order ID "<<target_order_id<<"\n";
            asks.erase(ask_it);
            return;
        }

        // std::cout<<"[ERROR] Cannot cancel: Order ID "<<target_order_id<<" not found or already filled. \n";
    }

    void OrderBook::print_book() const{
        std::cout<<"\n=== CURRENT ORDER BOOK ===\n";
        std::cout<<"---ASKS (Sellers) ---\n";
        for(auto it=asks.begin(); it!=asks.end(); it++){
            std::cout<<"Price: "<<it->price<<" | Quantity: "<<it->quantity<<" | ID: "<<it->order_id<<"\n";
        }
        std::cout<<"----------------\n";
        std::cout<<"--- BIDS (Buyers) ---\n";
        for(const auto& bid:bids){
            std::cout<<"Price: "<<bid.price<<" | Quantity: "<<bid.quantity<<" | ID: "<<bid.order_id<<"\n";
        }
        std::cout<<"================\n";
    }
}