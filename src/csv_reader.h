#pragma once
#include "orderbook.h"
#include<string>

 class CSVReader{
    public:
        static void load_orders(const std::string& filepath, OrderBook& book);
 };