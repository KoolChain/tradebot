#pragma once


#include "Database.h"
#include "Exchange.h"


namespace ad {
namespace tradebot {


struct Trader
{
    int cancelOpenOrders();

    std::string name;
    Pair pair;
    Database database;
    Exchange binance;
};


} // namespace tradebot
} // namespace ad
