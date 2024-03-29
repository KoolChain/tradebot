#include <binance/Api.h>

#include <tradebot/Exchange.h>
#include <tradebot/Logging.h>
#include <tradebot/Order.h>

#include <fstream>
#include <iostream>
#include <sstream>


using namespace ad;

int main(int argc, char * argv[])
{
    if (argc != 5)
    {
        std::cerr << "Usage: " << argv[0] << " secretsfile order_exchange_id base quote\n";
        return EXIT_FAILURE;
    }

    spdlog::set_level(spdlog::level::trace);

    tradebot::Exchange exchange{
        binance::Api{std::ifstream{argv[1]}}
    };

    tradebot::Pair pair{argv[3], argv[4]};
    const long exchangeId = std::stol(argv[2]);
    spdlog::info("Query order {} for {}.", exchangeId, pair.symbol());

    Json orderJson =
        *exchange.restApi.queryOrderForExchangeId(pair.symbol(), exchangeId).json;

    // Most values are dummy, only matters:
    // * symbol pair
    // * exchange id
    // * activation time
    tradebot::Order epochOrder{
        "tradelist",
        pair.base,
        pair.quote,
        jstod(orderJson.at("origQty")),
        0.,
        orderJson.at("side") == tradebot::Side::Sell ? tradebot::Side::Sell : tradebot::Side::Buy,
    };

    spdlog::info("Reconstructed order for {} {} at {} {}. Client order id: '{}', exchange status: {}, executed quantity: {}",
            epochOrder.baseAmount,
            epochOrder.base,
            epochOrder.executionRate,
            epochOrder.quote,
            orderJson.at("clientOrderId"),
            orderJson.at("status"),
            orderJson.at("executedQty")
    );

    epochOrder.activationTime = 10000.; // compensate for the margin hardcoded in accumulateTradesFor
    epochOrder.exchangeId = exchangeId;
    epochOrder.id = 0; // Dummy impossible DB id, so it does not throw on clientId();

    tradebot::Fulfillment fulfill = exchange.accumulateTradesFor(epochOrder);

    spdlog::info("Result: {}", boost::lexical_cast<std::string>(fulfill));

    return EXIT_SUCCESS;
}
