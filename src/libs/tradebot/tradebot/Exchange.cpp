#include "Exchange.h"

#include <spdlog/spdlog.h>

#include <boost/asio/post.hpp>

#include <functional>


namespace ad {
namespace tradebot {


const std::chrono::minutes LISTEN_KEY_REFRESH_PERIOD{30};
// Good for testing
//const std::chrono::milliseconds LISTEN_KEY_REFRESH_PERIOD{100};


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
            spdlog::warn("Market order '{}' for {} {} at {} {} is expired.",
                         aOrder.getIdentity(),
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
                             aOrder.getIdentity());
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
        assertExchangeIdConsistency(aOrder, (*response.json));
        return *response.json;
    }
    else
    {
        unhandledResponse(response, "query order");
    }
}


// listAccountTrades* return an empty array (not an error status) when there are no trades matching
Fulfillment Exchange::accumulateTradesFor(const Order & aOrder, int aPageSize)
{
    // Note:
    // It sporadically occured that the accumulated trades did not reach the total amount of the order.
    // I am not sure what causes it, so let's start listing trades from some time before the recorded
    // order activatin time, see if that situation ever occurs again.
    static const MillisecondsSinceEpoch TRADE_MARGIN = 10000;

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
                spdlog::trace("Trade {} for {} {} matching order '{}'.",
                              trade.at("id").get<long>(),
                              fill.amountBase,
                              aOrder.base,
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
    else if (result.amountBase != aOrder.amount)
    {
        spdlog::critical("Accumulated trades for order '{}' amount to {} {}, but the order was for {} {}.",
                         aOrder.getIdentity(),
                         result.amountBase,
                         aOrder.base,
                         aOrder.amount,
                         aOrder.base
                         );
        throw std::logic_error{"Trades accumulation does not match the order amount."};
    }

    return result;
}


void onStreamReceive(const std::string & aMessage, Exchange::UserStreamCallback aOnExecutionReport)
{
    Json json = Json::parse(aMessage);
    if (json.at("e") == "executionReport")
    {
        aOnExecutionReport(std::move(json));
    }
}


// IMPORTANT: Will execute the HTTP PUT query on the websocket io_context thread
// So it introduces concurrent execution of http requests
// (potentially complicating proper implementation of "quotas observation and waiting periods").
// TODO potentially have it post the request to be executed on the main thread.
void Exchange::Stream::onListenKeyTimer(const boost::system::error_code & aErrorCode)
{
    if (aErrorCode == boost::asio::error::operation_aborted)
    {
        spdlog::debug("Listen key refresh timer aborted.");
        return;
    }
    else if (aErrorCode)
    {
        spdlog::error("Error on listen key refresh timer: {}. Will try to go on.", aErrorCode.message());
    }

    // timer closed value is changed in the same thread running io_context
    // so it cannot race,
    // and as importantly it cannot change value "in the middle" of the if body.
    if (! intendedClose)
    {
        restApi.pingSpotListenKey();
        keepListenKeyAlive.expires_after(LISTEN_KEY_REFRESH_PERIOD);
        keepListenKeyAlive.async_wait(std::bind(&Exchange::Stream::onListenKeyTimer,
                                                this,
                                                std::placeholders::_1));
    }
    else
    {
        spdlog::debug("Timer was closed while the handler was already queued. Not restarting it.");
    }
}


Exchange::Stream::Stream(binance::Api & aRestApi,
                         Exchange::UserStreamCallback aOnExecutionReport) :
    restApi{aRestApi},
    listenKey{restApi.createSpotListenKey().json->at("listenKey")},
    websocket{
        // On connect
        [this]()
        {
            {
                std::scoped_lock<std::mutex> lock{mutex};
                status = Connected;
            }
            statusCondition.notify_one();

            // Cannot be done at the start of the thread directly
            // because it would still block the run when the websocket cannot connect
            keepListenKeyAlive.async_wait(std::bind(&Exchange::Stream::onListenKeyTimer,
                                                    this,
                                                    std::placeholders::_1));

        },
        // On Message
        [onReport = std::move(aOnExecutionReport)](const std::string & aMessage)
        {
            onStreamReceive(aMessage, onReport);
        }
    },
    keepListenKeyAlive{
        websocket.exposeContextDetail(),
        LISTEN_KEY_REFRESH_PERIOD},
    thread{
        [this]()
        {
            websocket.run(restApi.getEndpoints().websocketHost,
                          restApi.getEndpoints().websocketPort,
                          "/ws/" + listenKey);

            if (! intendedClose)
            {
                spdlog::warn("User data stream websocket closed without application consent.");
            }

            {
                std::scoped_lock<std::mutex> lock{mutex};
                status = Done;
            }
            statusCondition.notify_one();
        }
    }
{}


Exchange::Stream::~Stream()
{
    // This object cannot be destroyed until the io_context is done running
    // so it is safe to capture it in a lambda executed on the io_context thread.
    boost::asio::post(
        keepListenKeyAlive.get_executor(),
        [this]
        {
            // Cancel any pending wait on the timer
            // (otherwise the io_context::run() would still block on already waiting handlers)
            keepListenKeyAlive.cancel();
            // There are conditions where the timer handler might not be waiting anymore
            // even though the cancellation in happening in the same thread as the completion handler
            // (notably, completion handler already pending in the io_context queue, just after this lambda)
            // So it would not be cancelled by the call to cancel().
            // see: https://stackoverflow.com/a/43169596/1027706
            intendedClose = true;
        });

    if (restApi.closeSpotListenKey(listenKey).status != 200)
    {
        spdlog::critical("Cannot close the current spot listen key, cannot throw in a destructor.");
        // See note below.
        //websocket.async_close();
    }
    // The close above should result in the websocket closing, so the thread terminating.
    // NOTE: I expected that closing the listen key would have the server close the websocket.
    //       Apparently this is not the case, so close it manually anyway.
    websocket.async_close();
    thread.join();
    spdlog::debug("User stream closed successfully.");
}



bool Exchange::openUserStream(UserStreamCallback aOnExecutionReport)
{
    spotUserStream.emplace(restApi, std::move(aOnExecutionReport));
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


} // namespace tradebot
} // namespace ad
