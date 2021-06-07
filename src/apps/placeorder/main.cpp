#include <binance/Api.h>

#include <tradebot/Database.h>
#include <tradebot/Exchange.h>
#include <tradebot/Order.h>
#include <tradebot/Trader.h>

#include <spdlog/spdlog.h>

#include <boost/lexical_cast.hpp>

#include <fstream>
#include <iostream>
#include <sstream>


using namespace ad;

int main(int argc, char * argv[])
{
    if (argc != 6)
    {
        std::cerr << "Usage: " << argv[0] << " secretsfile side base quote amount\n";
        return EXIT_FAILURE;
    }

    spdlog::set_level(spdlog::level::trace);

    tradebot::Pair pair{argv[3], argv[4]};
    tradebot::Side side{tradebot::readSide(argv[2])};
    Decimal amount = std::stod(argv[5]);

    spdlog::info("Placing market {} order for {} {}.",
            boost::lexical_cast<std::string>(side),
            amount,
            pair.base);

    tradebot::Trader trader{
        "placeorder",
        {argv[3], argv[4]},
        tradebot::Database{":memory:"},
        tradebot::Exchange{
            binance::Api{std::ifstream{argv[1]}, binance::Api::gTestNet}
        }
    };

    tradebot::Order order{
            trader.name,
            pair.base,
            pair.quote,
            amount,
            0.,
            side
    };

    trader.fillNewMarketOrder(order);

    spdlog::info("Executed at price: {} {}.", order.executionRate, order.quote);

    return EXIT_SUCCESS;
}
