#include "Exchange.h"

#include <spdlog/spdlog.h>


namespace ad {
namespace tradebot {


#define unhandledResponse(aResponse, aContext) \
{ \
    spdlog::critical("Unexpected response status {} on {}.", aResponse.status, aContext); \
    throw std::logic_error("Unhandled web API response."); \
}

std::string Exchange::getOrderStatus(const Order & aOrder)
{
    binance::Response response = restApi.queryOrder(aOrder.symbol(), aOrder.clientId());

    if (response.status == 200)
    {
        return (*response.json)["status"];
    }
    else if ((response.status == 400)
             && ((*response.json)["code"] == -2013))
    {
            return "NOTEXISTING";
    }

    unhandledResponse(response, "get order status");
}


Decimal Exchange::getCurrentAveragePrice(const Pair & aPair)
{
    binance::Response response = restApi.getCurrentAveragePrice(aPair.symbol());

    if (response.status == 200)
    {
        const Json & json = *response.json;
        return std::stod(json["price"].get<std::string>());
    }
    else
    {
        unhandledResponse(response, "current average price");
    }
}


template<class T_order>
binance::Response placeOrderImpl(const T_order & aBinanceOrder, Order & aOrder, binance::Api & aRestApi)
{
    binance::Response response = aRestApi.placeOrderTrade(aBinanceOrder);

    if (response.status == 200)
    {
        const Json & json = *response.json;
        aOrder.status = Order::Status::Active;
        aOrder.activationTime = json["transactTime"];
        aOrder.exchangeId = json["orderId"];
    }

    return response;
}

// Place order returns 400 -1013 if the price is above the symbol limit.
Order Exchange::placeOrder(Order & aOrder, Execution aExecution)
{
    binance::Response response;
    switch(aExecution)
    {
        case Execution::Market:
            response = placeOrderImpl(to_marketOrder(aOrder), aOrder, restApi);
            break;
        case Execution::Limit:
            response = placeOrderImpl(to_limitOrder(aOrder), aOrder, restApi);
            break;
    }

    if (response.status == 200)
    {
        return aOrder;
    }
    else
    {
        unhandledResponse(response, "place order");
    }
}


std::optional<FulfilledOrder> Exchange::fillMarketOrder(Order & aOrder)
{
    binance::Response response = placeOrderImpl(to_marketOrder(aOrder), aOrder, restApi);
    if (response.status == 200)
    {
        const Json & json = *response.json;
        if (json["status"] == "FILLED")
        {
            return fulfillFromQuery(aOrder, json);
        }
        else if (json["status"] == "EXPIRED")
        {
            spdlog::warn("Market order '{}' for {} {} at {} {} is expired.",
                         static_cast<const std::string &>(aOrder.clientId()),
                         aOrder.amount,
                         aOrder.base,
                         aOrder.fragmentsRate,
                         aOrder.quote
                    );
            return {};
        }
        else
        {
            spdlog::critical("Unhandled status '{}' when placing market order '{}'.",
                             json["status"],
                             static_cast<const std::string &>(aOrder.clientId()));
            throw std::logic_error{"Unhandled market order status in response."};
        }
    }
    else
    {
        unhandledResponse(response, "fill market order");
    }
}


// Return 400 -2011 if the provided order is not present to be cancelled
bool Exchange::cancelOrder(const Order & aOrder)
{
    binance::Response response = restApi.cancelOrder(aOrder.symbol(), aOrder.clientId());

    if (response.status == 200)
    {
        return true;
    }
    else if (response.status == 400)
    {
        if ((*response.json)["code"] == -2011)
        {
            spdlog::trace("Order '{}' was not present in the exchange.",
                          std::string{aOrder.clientId()});
            return false;
        }
    }

    unhandledResponse(response, "cancel order");
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
    binance::Response response = restApi.cancelAllOpenOrders(aPair.symbol());

    if (response.status == 200)
    {
        std::vector<binance::ClientId> result;
        Json json = *response.json;
        std::transform(json.begin(), json.end(), std::back_inserter(result),
                       [](const auto & element)
                        {
                            return binance::ClientId{element["origClientOrderId"]};
                        });
        return result;
    }
    else if ((response.status == 400)
             && ((*response.json)["code"] == -2011))
    {
        return {};
    }

    unhandledResponse(response, "cancel all open orders");
}


std::vector<binance::ClientId> Exchange::listOpenOrders(const Pair & aPair)
{
    binance::Response response = restApi.listOpenOrders(aPair.symbol());

    if (response.status == 200)
    {
        std::vector<binance::ClientId> result;
        Json json = *response.json;

        std::transform(json.begin(), json.end(), std::back_inserter(result),
                       [](const auto & element)
                        {
                            return binance::ClientId{element["clientOrderId"]};
                        });
        return result;
    }
    else if ((response.status == 400)
             && ((*response.json)["code"] == -2011))
    {
        return {};
    }

    unhandledResponse(response, "list open orders");
}


Json Exchange::queryOrder(const Order & aOrder)
{
    binance::Response response = restApi.queryOrder(aOrder.symbol(), aOrder.clientId());
    if (response.status == 200)
    {
        return *response.json;
    }
    else
    {
        unhandledResponse(response, "query order");
    }
}


} // namespace tradebot
} // namespace ad
