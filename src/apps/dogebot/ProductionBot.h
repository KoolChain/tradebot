#pragma once


#include "EventLoop.h"

#include <tradebot/Order.h>
#include <tradebot/Trader.h>


namespace ad {
namespace trade {


struct ProductionBot
{
    int run();

    void onAggregateTrade(Json aMessage);

    tradebot::Trader trader;
    EventLoop mainLoop;
};


} // namespace trade
} // namespace ad
