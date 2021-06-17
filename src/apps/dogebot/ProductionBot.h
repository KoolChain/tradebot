#pragma once


#include <tradebot/Trader.h>


namespace ad {
namespace trade {


struct ProductionBot
{
    int run();

    tradebot::Trader trader;
};


} // namespace trade
} // namespace ad
