#include "ProductionBot.h"

#include <tradebot/Exchange.h>

#include <spdlog/spdlog.h>

#include <boost/algorithm/string.hpp>
#include <boost/asio/post.hpp>

#include <cstdlib>


namespace ad {
namespace trade {


bool IntervalTracker::update(Decimal aLatestPrice)
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

    return newLowerBound != std::exchange(lowerBound, newLowerBound);
}


void IntervalTracker::reset()
{
    lowerBound = ladder.crend();
}


void ProductionBot::onAggregateTrade(Json aMessage)
{
    Decimal latestPrice{aMessage.at("p").get<std::string>()};

    if (tracker.update(latestPrice))
    {
        spdlog::info("Latest price {} changed the current ladder interval.",
                latestPrice);
        boost::asio::post(mainLoop.getContext(), [this, latestPrice]()
                {
                    trader.makeAndFillProfitableOrders(latestPrice);
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
    trader.cleanup();

    tracker.reset();
    connectMarketStream();

    mainLoop.run();

    return EXIT_SUCCESS;
}


} // namespace trade
} // namespace ad
