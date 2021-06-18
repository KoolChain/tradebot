#include "ProductionBot.h"

#include <tradebot/Exchange.h>

#include <spdlog/spdlog.h>

#include <boost/algorithm/string.hpp>
#include <boost/asio/post.hpp>

#include <cstdlib>


namespace ad {
namespace trade {


void ProductionBot::onAggregateTrade(Json aMessage)
{
    spdlog::info("Aggregate trade:\n{}", aMessage.dump(4));
    boost::asio::post(mainLoop.getContext(), [this, rate = aMessage.at("p").get<std::string>()]()
            {
                trader.makeAndFillProfitableOrders(Decimal{rate});
            });
}


int ProductionBot::run()
{
    trader.cleanup();

    trader.exchange.openMarketStream(std::bind(&ProductionBot::onAggregateTrade, this, std::placeholders::_1),
                                     boost::to_lower_copy(trader.pair.symbol()) + "@aggTrade");

    mainLoop.run();

    return EXIT_SUCCESS;
}


} // namespace trade
} // namespace ad
