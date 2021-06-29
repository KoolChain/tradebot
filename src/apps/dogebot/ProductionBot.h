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


/// \brief Hardcoded to dump stats everyday at midnight
struct StatsWriter
{
    void start();

private:
    /// \return The time_point when the stats should be written next.
    void initializeAndCatchUp();

    void writeStats();

    void async_wait();

    void onTimer(const boost::system::error_code & aErrorCode);

public:
    tradebot::Trader & trader;
    boost::asio::steady_timer timer;
    std::chrono::time_point<std::chrono::steady_clock> nextTime{};
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
    StatsWriter stats{trader,
                      boost::asio::steady_timer{mainLoop.getContext()}};
};


} // namespace trade
} // namespace ad
