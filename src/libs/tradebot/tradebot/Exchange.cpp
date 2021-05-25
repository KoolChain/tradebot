#include "Exchange.h"

#include <spdlog/spdlog.h>


namespace ad {
namespace tradebot {


std::string Exchange::getOrderStatus(Order aOrder, const std::string & aTraderName)
{
    binance::Response response = restApi.queryOrder(aOrder.symbol(), aOrder.clientId(aTraderName));

    if (response.status == 200)
    {
        return (*response.json)["status"];
    }
    else if (response.status == 400)
    {
        if ((*response.json)["code"] == -2013)
        {
            return "NOTEXISTING";
        }
        else
        {
            spdlog::error("Unexpected status {} with binance error code {}.", response.status, (*response.json)["code"]);
        }
    }
    else
    {
        spdlog::error("Unexpected status {}.", response.status);
    }

    return "UNEXPECTED";
}


// Cancel all order return 400 -2011 if no order is present to be cancelled.
// If they are present, returned in a list
// [
//    {
//        "clientOrderId": "5W7sG4b9nnqQrFZd6p1i5n",
//        "cummulativeQuoteQty": "0.00000000",
//        "executedQty": "0.00000000",
//        "orderId": 2179201,
//        "orderListId": -1,
//        "origClientOrderId": "5sdqJoJycVBAEg3rmQbbsF",
//        "origQty": "0.01000000",
//        "price": "100000.00000000",
//        "side": "SELL",
//        "status": "CANCELED",
//        "symbol": "BTCUSDT",
//        "timeInForce": "GTC",
//        "type": "LIMIT"
//    }
//]
std::vector<binance::ClientId> Exchange::cancelAllOpenOrders(const Pair & aPair)
{
    std::vector<binance::ClientId> result;

    binance::Response response = restApi.cancelAllOpenOrders(aPair.symbol());

    if (response.status == 200)
    {
        Json json = *response.json;
        std::transform(json.begin(), json.end(), std::back_inserter(result),
                       [](const auto & element)
                        {
                            return binance::ClientId{element["origClientOrderId"]};
                        });
    }
    else if (response.status == 400)
    {
        if ((*response.json)["code"] == -2011)
        {
            // normal empty return situation
        }
        else
        {
            spdlog::error("Unexpected status {} with binance error code {}.",
                          response.status,
                          (*response.json)["code"]);
        }
    }
    else
    {
        spdlog::error("Unexpected status {}.", response.status);
    }
    return result;
}

// Place order returns 400 -1013 if the price is above the symbol limit.

} // namespace tradebot
} // namespace ad
