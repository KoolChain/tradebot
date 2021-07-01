#include "ProductionBot.h"

#include <tradebot/Exchange.h>
#include <tradebot/Logging.h>

#include <boost/algorithm/string.hpp>
#include <boost/asio/post.hpp>

#include <cstdlib>


namespace ad {
namespace trade {


// The bot is not doing anything time sensitive, since all orders are
// placed as Limit FOK.
// As there were several API errors on test net:
// "Client error -1021: Timestamp for this request is outside of the recvWindow."
// So this timer has been made larger.
static const std::chrono::milliseconds gProdbotReceiveWindow{15000};


std::optional<Interval> IntervalTracker::update(Decimal aLatestPrice)
{
    trade::Ladder::const_reverse_iterator newLowerBound =
        std::find_if(ladder.crbegin(), ladder.crend(), [&, this](auto stop)
            {
                return stop <= aLatestPrice;
            });

    if (   newLowerBound == ladder.crend()
        || newLowerBound == ladder.crbegin())
    {
        if (ladder.size())
        {
            spdlog::warn("Price {} is out of the ladder interval [{}, {}].",
                    aLatestPrice,
                    ladder.front(),
                    ladder.back());
        }
        else
        {
            spdlog::critical("Empty ladder is not allowed in an interval tracker.");
            throw std::runtime_error{"The interval tracker ladder is empty."};
        }
    }

    if (newLowerBound != std::exchange(lowerBound, newLowerBound))
    {
        return Interval{lowerBound == ladder.crend()   ? *(lowerBound-1) : *lowerBound,
                        lowerBound == ladder.crbegin() ?     *lowerBound : *(lowerBound-1)};
    }
    else
    {
        return {};
    }
}


void IntervalTracker::reset()
{
    lowerBound = ladder.crend();
}


void StatsWriter::start()
{
    initializeAndCatchUp();
    timer.expires_at(nextTime);
    spdlog::debug("Statistics timer first expiry set at '{}'.",
                  timeToString(nextTime));
    async_wait();
}


void StatsWriter::initializeAndCatchUp()
{
    MillisecondsSinceEpoch todayMidnight = getMidnightTimestamp();
    if (trader.database.countBalances(todayMidnight) == 0)
    {
        // there are no balance recorded for the current day, write one now
        // This will cover both the first balance on to be written on first launch
        // as well as catching up in case the application was not up at the timer expiry
        spdlog::info("Balance statistics for the current day were not found, recording now.");
        writeStats();
    }

    //nextTime = std::chrono::milliseconds{todayMidnight} + std::chrono::hours{24};
    nextTime =
        std::chrono::system_clock::time_point{std::chrono::milliseconds{todayMidnight}}
        + std::chrono::hours{24};
}

void StatsWriter::async_wait()
{
    timer.async_wait(std::bind(&StatsWriter::onTimer, this, std::placeholders::_1));
}


void StatsWriter::writeStats()
{
    trader.recordBalance(getTimestamp());
}


void StatsWriter::onTimer(const boost::system::error_code & aErrorCode)
{
    spdlog::trace("Statistics timer event.");
    if (aErrorCode == boost::asio::error::operation_aborted)
    {
        spdlog::debug("Statistics timer aborted.");
        return;
    }
    else if (aErrorCode)
    {
        spdlog::error("Error on statistics timer: {}. Will try to go on.", aErrorCode.message());
    }
    else
    {
        nextTime += std::chrono::hours{24};
        writeStats();
    }

    timer.expires_at(nextTime);
    async_wait();
}


void ProductionBot::onAggregateTrade(Json aMessage)
{
    Decimal latestPrice{aMessage.at("p").get<std::string>()};

    if (auto optionalInterval = tracker.update(latestPrice))
    {
        spdlog::info("Latest price {} changed the current ladder interval.",
                latestPrice);
        // This executes on the stream thread, increment each time an interval change is detected
        ++intervalChangeSemaphore;

        boost::asio::post(mainLoop.getContext(), [this, latestPrice, interval=*optionalInterval]()
                {
                    spdlog::info("Start handling new interval [{}, {}].",
                                 interval.front, interval.back);
                    // Matches the increment, with a decrement occuring in the main thread
                    // where all the order filling takes place.
                    // Combined with the predicate below, it allows to stop trying to fill
                    // in case a new interval change is detected before some orders have completed.
                    --intervalChangeSemaphore;

                    trader.makeAndFillProfitableOrders(
                            interval,
                            symbolFilters,
                            [this]
                            {
                                return intervalChangeSemaphore == 0;
                            });
                });
    }
}

void ProductionBot::connectMarketStream()
{
    trader.exchange.openMarketStream(boost::to_lower_copy(trader.pair.symbol()) + "@aggTrade",
                                     std::bind(&ProductionBot::onAggregateTrade,
                                               this,
                                               std::placeholders::_1),
                                     [this]()
                                     {
                                        boost::asio::post(mainLoop.getContext(),
                                                          std::bind(&ProductionBot::connectMarketStream,
                                                                    this));
                                     });
}


int ProductionBot::run()
{
    trader.exchange.restApi.setReceiveWindow(gProdbotReceiveWindow);
    trader.cleanup();

    stats.start();

    tracker.reset();
    connectMarketStream();

    mainLoop.run();

    return EXIT_SUCCESS;
}


} // namespace trade
} // namespace ad
