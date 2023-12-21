#include "Exchange.h"
#include "Logging.h"

#include <trademath/DecimalLog.h>


namespace ad {
namespace tradebot {


const std::chrono::minutes LISTEN_KEY_REFRESH_PERIOD{30};


#define unhandledResponse(aResponse, aContext) \
{ \
    spdlog::critical("Unexpected response status {} on {}.", aResponse.status, aContext); \
    throw std::logic_error("Unhandled web API response."); \
}


#define assertExchangeIdConsistency(dOrder, dJson) \
{                                                                               \
    if ((dOrder).exchangeId != -1 /* -1 could happend for a 'Sending' order */  \
        && (dOrder).exchangeId != (dJson).at("orderId"))                        \
    {                                                                           \
        spdlog::critical("Order instance '{}' does not match the id returned by the exchange: {}.", \
                         (dOrder).getIdentity(),                                \
                         (dJson)["orderId"].get<long>());                    \
        throw std::logic_error("Recorded order exchange id does not match with the id returned by the exchange.");  \
    }                                                                           \
}


std::string Exchange::getOrderStatus(const Order & aOrder)
{
    binance::Response response = restApi.queryOrder(aOrder.symbol(), aOrder.clientId());

    if (response.status == 200)
    {
        assertExchangeIdConsistency(aOrder, (*response.json));
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
        return jstod(json["price"]);
    }
    else
    {
        unhandledResponse(response, "current average price");
    }
}


Json Exchange::getExchangeInformation(std::optional<Pair> aPair)
{
    binance::Response response = aPair ?
        restApi.getExchangeInformation(aPair->symbol())
        : restApi.getExchangeInformation();


    if (response.status == 200)
    {
        return *response.json;
    }
    else
    {
        unhandledResponse(response, "exchange information");
    }
}


std::pair<Decimal, Decimal> Exchange::getBalance(Pair aPair)
{
    binance::Response response = restApi.getAccountInformation();

    if (response.status == 200)
    {
        std::pair<Decimal, Decimal> result{0, 0};
        for (const Json & asset : response.json->at("balances"))
        {
            if      (asset.at("asset") == aPair.base)  result.first = jstod(asset.at("free"));
            else if (asset.at("asset") == aPair.quote) result.second = jstod(asset.at("free"));
        }
        // If the asset is not listed, it means the balance is zero.
        return result;
    }
    else
    {
        unhandledResponse(response, "account information");
    }
}


SymbolFilters Exchange::queryFilters(const Pair & aPair)
{
    SymbolFilters result;
    Json filtersArray = getExchangeInformation(aPair)["symbols"][0]["filters"];
    for (const auto & filter : filtersArray)
    {
        if (filter.at("filterType") == "PRICE_FILTER")
        {
            result.price = SymbolFilters::ValueDomain{
                Decimal{filter["minPrice"].get<std::string>()},
                Decimal{filter["maxPrice"].get<std::string>()},
                Decimal{filter["tickSize"].get<std::string>()},
            };
        }
        else if (filter.at("filterType") == "LOT_SIZE")
        {
            result.amount = SymbolFilters::ValueDomain{
                Decimal{filter["minQty"].get<std::string>()},
                Decimal{filter["maxQty"].get<std::string>()},
                Decimal{filter["stepSize"].get<std::string>()},
            };
        }
        else if (filter.at("filterType") == "NOTIONAL")
        {
            result.minimumNotional = Decimal{filter["minNotional"].get<std::string>()};
        }
    }
    return result;
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

        // Sanity check
        {
            if (json.at("clientOrderId") != aOrder.clientId())
            {
                spdlog::critical("New order response contains client-id {}, while \"{}\" was placed.",
                                 json.at("clientOrderId"),
                                 static_cast<const std::string &>(aOrder.clientId()));
                throw std::runtime_error{"Inconsistant new order response, client-id is not matching."};
            }
        }
    }

    return response;
}

// Place order returns 400 -1013 if the price is above the symbol limit.
Order & Exchange::placeOrder(Order & aOrder, Execution aExecution)
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
        case Execution::LimitFok:
            response = placeOrderImpl(to_limitFokOrder(aOrder), aOrder, restApi);
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


template <class T_order>
std::optional<FulfilledOrder> fillOrderImpl(const T_order & aBinanceOrder,
                                            Order & aOrder,
                                            binance::Api & aRestApi,
                                            const std::string & aOrderType)
{
    binance::Response response = placeOrderImpl(aBinanceOrder, aOrder, aRestApi);
    if (response.status == 200)
    {
        const Json & json = *response.json;
        if (json["status"] == "FILLED")
        {
            Fulfillment fulfillment =
                std::accumulate(json.at("fills").begin(),
                                json.at("fills").end(),
                                Fulfillment{},
                                [&aOrder](Fulfillment & fulfillment, const auto & fillJson)
                                {
                                    return fulfillment.accumulate(Fulfillment::fromFillJson(fillJson), aOrder);
                                });
            // The new order fills do not contain the quote quantities, patch it manually
            fulfillment.amountQuote = jstod(json.at("cummulativeQuoteQty"));
            return fulfill(aOrder, json, fulfillment);
        }
        else if (json["status"] == "EXPIRED")
        {
            // TODO the fragment rate is not necessarily the order rate.
            // This is confusing.
            spdlog::warn("{} order '{}' for {} {} at {} {} is expired.",
                         aOrderType,
                         aOrder.getIdentity(),
                         aOrder.baseAmount,
                         aOrder.base,
                         aOrder.fragmentsRate,
                         aOrder.quote
                    );
            return {};
        }
        else
        {
            spdlog::critical("Unhandled status '{}' when placing {} order '{}'.",
                             json["status"],
                             aOrderType,
                             aOrder.getIdentity());
            throw std::logic_error{"Unhandled order status in response."};
        }
    }
    else
    {
        unhandledResponse(response, aOrderType + " order");
    }
}


std::optional<FulfilledOrder> Exchange::fillMarketOrder(Order & aOrder)
{
    return fillOrderImpl(to_marketOrder(aOrder), aOrder, restApi, "market");
}


std::optional<FulfilledOrder> Exchange::fillLimitFokOrder(Order & aOrder,
                                                          std::optional<Decimal> aExplicitLimitRate)
{
    binance::LimitOrder limitOrder = to_limitFokOrder(aOrder);
    if (aExplicitLimitRate)
    {
        limitOrder.price = *aExplicitLimitRate;
    }
    return fillOrderImpl(limitOrder, aOrder, restApi, "limit fok");
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
        assertExchangeIdConsistency(aOrder, (*response.json));
        return *response.json;
    }
    else
    {
        unhandledResponse(response, "query order");
    }
}


std::optional<Json> Exchange::tryQueryOrder(const Order & aOrder,
                                            int aAttempts,
                                            std::chrono::milliseconds aDelay)
{
    return tryQueryOrder(aOrder, [](const Json &){return true;}, aAttempts, aDelay);
}


std::optional<Json> Exchange::tryQueryOrder(const Order & aOrder,
                                            std::function<bool(const Json &)> aPredicate,
                                            int aAttempts,
                                            std::chrono::milliseconds aDelay)
{
    --aAttempts;

    binance::Response response = restApi.queryOrder(aOrder.symbol(), aOrder.clientId());
    if (response.status == 200)
    {
        Json json = (*response.json);
        // Will exit by throwing when no attempts are left and the exchange ids still don't match
        if (aOrder.exchangeId == -1 /* -1 could happen for a 'Sending' order */
            || aOrder.exchangeId == json.at("orderId"))
        {
            if (aPredicate(json))
            {
                return json;
            }
            else if (aAttempts > 0)
            {
                spdlog::warn("Returned JSON for order instance '{}' did not satisfy the predicate."
                             " {} attempt(s) left.",
                             aOrder.getIdentity(),
                             aAttempts);
            }
            else // no more attempts
            {
                spdlog::critical("Returned JSON for order instance '{}' did not satisfy the predicate.",
                                 aOrder.getIdentity());
                throw std::logic_error{"JSON returned by the exchange does not satisfy predicate."};
            }
        }
        else if (aAttempts > 0)
        {
            spdlog::warn("Order instance '{}' does not match the id returned by the exchange: {}."
                         " {} attempt(s) left.",
                         aOrder.getIdentity(),
                         json.at("orderId"),
                         aAttempts);
        }
        else // no more attempts
        {
            // Will take care of log and exception
            assertExchangeIdConsistency(aOrder, (*response.json));
        }
    }
    else if (response.status == 400
             && response.json->at("code") == -2013)
    {
        spdlog::warn("Order instance '{}' could not be queried, exchange returned: {}."
                     " {} attempt(s) left.",
                     aOrder.getIdentity(),
                     response.json->at("msg"),
                     aAttempts);
    }
    else
    {
        unhandledResponse(response, "try query order");
    }

    // If status was still 400 -2013 after all the attempts, will fall to the else
    if (aAttempts > 0)
    {
        std::this_thread::sleep_for(aDelay);
        return tryQueryOrder(aOrder, std::move(aPredicate), aAttempts, aDelay);
    }
    else
    {
        return {};
    }
}

// listAccountTrades* return an empty array (not an error status) when there are no trades matching
Fulfillment Exchange::accumulateTradesFor(const Order & aOrder, int aPageSize)
{
    // Note:
    // It sporadically occured that the accumulated trades did not reach the total amount of the order.
    // I am not sure what causes it, so let's start listing trades from some time before the recorded
    // order activatin time, see if that situation ever occurs again.
    //static const MillisecondsSinceEpoch TRADE_MARGIN = 10000;
    // TODO can we remove it?
    // Causes an issue with "sending" order (for which the activation time will be 0)
    static const MillisecondsSinceEpoch TRADE_MARGIN = 0;

    Fulfillment result;
    binance::Response response =
        restApi.listAccountTradesFromTime(aOrder.symbol(),
                                          aOrder.activationTime-TRADE_MARGIN,
                                          aPageSize);
    long lastTradeId = 0;

    while(response.status == 200 && (!(*response.json).empty()))
    {
        for(const auto & trade : *response.json)
        {
            if (trade.at("orderId") == aOrder.exchangeId)
            {
                Fulfillment fill = Fulfillment::fromTradeJson(trade);
                result.accumulate(fill, aOrder);
                spdlog::trace("Trade {} for {} {} / {} {} matching order '{}'.",
                              trade.at("id").get<long>(),
                              fill.amountBase,
                              aOrder.base,
                              fill.amountQuote,
                              aOrder.quote,
                              aOrder.getIdentity());
            }
            lastTradeId = trade.at("id");
            // TODO might return as soon as the order.amount is matched,
            // yet that would make testing pagination even more of a pain.
        }
        response = restApi.listAccountTradesFromId(aOrder.symbol(), lastTradeId+1, aPageSize);
    }

    if (response.status != 200)
    {
        unhandledResponse(response, "accumulates order trades");
    }
    else if (! isEqual(result.amountBase, aOrder.baseAmount))
    {
        spdlog::critical("Accumulated trades for order '{}' amount to {} {}, but the order was for {} {}.",
                         aOrder.getIdentity(),
                         result.amountBase,
                         aOrder.base,
                         aOrder.baseAmount,
                         aOrder.base
                         );
        throw std::logic_error{"Trades accumulation does not match the order amount."};
    }

    return result;
}


bool Exchange::openUserStream(Stream::ReceiveCallback aOnMessage,
                              Stream::UnintendedCloseCallback aOnUnintededClose)
{
    WebsocketDestination userStreamDestination{
        restApi.getEndpoints().websocketHost,
        restApi.getEndpoints().websocketPort,
        "/ws/" + restApi.createSpotListenKey().json->at("listenKey").get<std::string>()
    };

    spotUserStream.emplace(std::move(userStreamDestination),
                           std::move(aOnMessage),
                           std::move(aOnUnintededClose),
                           std::make_unique<RefreshTimer>(
                               // IMPORTANT: Will execute the HTTP PUT query on the timer io_context thread
                               // So it introduces concurrent execution of http requests
                               // (potentially complicating proper implementation of "quotas observation and waiting periods").
                               // TODO potentially have it post the request to be executed on the main thread.
                               std::bind(&binance::Api::pingSpotListenKey, restApi),
                               LISTEN_KEY_REFRESH_PERIOD
                           ));

    // Block until the websocket either connects or fails to do so.
    std::unique_lock<std::mutex> lock{spotUserStream->mutex};
    spotUserStream->statusCondition.wait(lock,
                                         [&stream = *spotUserStream]()
                                         {
                                             return stream.status != Stream::Initialize;
                                         });
    return spotUserStream->status == Stream::Connected;
}


void Exchange::closeUserStream()
{
    spotUserStream.reset();
}


bool Exchange::openMarketStream(const std::string & aStreamName,
                                Stream::ReceiveCallback aOnMessage,
                                Stream::UnintendedCloseCallback aOnUnintededClose)
{
    WebsocketDestination marketStreamDestination{
        restApi.getEndpoints().websocketHost,
        restApi.getEndpoints().websocketPort,
        "/ws/" + aStreamName
    };

    marketStream.emplace(std::move(marketStreamDestination),
                         std::move(aOnMessage),
                         std::move(aOnUnintededClose));

    // Block until the websocket either connects or fails to do so.
    std::unique_lock<std::mutex> lock{marketStream->mutex};
    marketStream->statusCondition.wait(lock,
                                       [&stream = *marketStream]()
                                       {
                                           return stream.status != Stream::Initialize;
                                       });
    return marketStream->status == Stream::Connected;
}


void Exchange::closeMarketStream()
{
    marketStream.reset();
}


} // namespace tradebot
} // namespace ad
