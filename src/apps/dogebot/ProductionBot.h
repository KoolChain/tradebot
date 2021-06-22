#pragma once


#include "EventLoop.h"

#include <tradebot/Order.h>
#include <tradebot/Trader.h>

#include <trademath/Interval.h>


namespace ad {
namespace trade {


struct IntervalTracker
{
    /// \return `true` if the latest price made the interval change.
    std::optional<Interval> update(Decimal aLatestPrice);

    void reset();

    trade::Ladder ladder;
    trade::Ladder::const_reverse_iterator lowerBound{ladder.crend()};
};

struct ProductionBot
{
    void connectMarketStream();

    int run();

    void onAggregateTrade(Json aMessage);

    tradebot::Trader trader;
    IntervalTracker tracker;

    // Automatically initialized
    std::atomic<int> intervalChangeSemaphore{0};
    tradebot::SymbolFilters symbolFilters{trader.queryFilters()};
    EventLoop mainLoop;
};


} // namespace trade
} // namespace ad
