#pragma once


#include "Fulfillment.h"
#include "Order.h"
#include "Stream.h"
#include "SymbolFilters.h"

#include <binance/Api.h>


namespace ad {
namespace tradebot {


enum class Execution
{
    Market,
    Limit,
    LimitFok,
};


struct Exchange
{
    /// \return "NOTEXISTING" if the exchange engine does not know the order, otherwise returns
    ///         the value from the exchange:
    ///         * NEW (Active order)
    ///         * CANCELED
    ///         * FILLED
    ///         * EXPIRED
    std::string getOrderStatus(const Order & aOrder);

    Decimal getCurrentAveragePrice(const Pair & aPair);

    Json getExchangeInformation(std::optional<Pair> aPair = {});

    SymbolFilters queryFilters(const Pair & aPair);

    Order & placeOrder(Order & aOrder, Execution aExecution);

    std::optional<FulfilledOrder> fillMarketOrder(Order & aOrder);

    std::optional<FulfilledOrder> fillLimitFokOrder(Order & aOrder);

    /// \return true if the order was cancelled, false otherwise
    ///         (because it was not present, already cancelled, etc.).
    bool cancelOrder(const Order & aOrder);

    std::vector<binance::ClientId> cancelAllOpenOrders(const Pair & aPair);

    std::vector<binance::ClientId> listOpenOrders(const Pair & aPair);

    Json queryOrder(const Order & aOrder);

    /// \brief Try to query the order in a loop, up to `aAttempts` times.
    ///
    /// It was observed during development that the exchange might report an order acceptation
    /// (via user data stream, or place order API), before it is ready to be queried on the API.
    /// Introducing repeated attempts with timers is a pragmatic workaround.
    ///
    /// The binance server order inconsistencies can take different forms.
    /// Another problem might be that the user stream reported the order filled, but the query API
    /// would still report it partially filled for some time.
    /// This is the motivation to introduce another mitigation measure via a predicate, that can
    /// be used to retry until the Json is in an expected state.
    ///
    /// \param aPredicate A Callable that takes a const reference to `Json`, and return `true` when
    /// the Json is in the expected state.
    //
    /// \param aAttempts Should be >= 1 to make any sense.
    ///
    /// \attention This will always tries at least once, even for 0 or negative `aAttempts`.
    ///
    /// \throw std::logic_error on unexpected http response status, or when
    /// no attempts are left but the exchange-id of `aOrder` still does not match the json response.
    std::optional<Json> tryQueryOrder(const Order & aOrder,
                                      std::function<bool(const Json &)> aPredicate = [](const Json &){return true;},
                                      int aAttempts = 5,
                                      std::chrono::milliseconds aDelay = std::chrono::milliseconds{60});

    /// \brief Overload to specify the retry parameters without losing the default predicate.
    std::optional<Json> tryQueryOrder(const Order & aOrder,
                                      int aAttempts,
                                      std::chrono::milliseconds aDelay = std::chrono::milliseconds{60});

    /// \param aPageSize The number of trades fetched with each API request.
    ///        Mainly usefull for writing tests.
    Fulfillment accumulateTradesFor(const Order & aOrder, int aPageSize=1000);

    /// \brief Blocks while opening a websocket to get spot user data stream.
    ///
    /// \return `true` if the websocket connected successfully, `false` otherwise.
    bool openUserStream(Stream::ReceiveCallback aOnMessage,
                        Stream::UnintendedCloseCallback aOnUnintededClose = [](){});
    void closeUserStream();

    /// \brief Blocks while opening a websocket to get market stream.
    ///
    /// \return `true` if the websocket connected successfully, `false` otherwise.
    bool openMarketStream(const std::string & aStreamName,
                          Stream::ReceiveCallback aOnMessage,
                          Stream::UnintendedCloseCallback aOnUnintededClose = [](){});
    void closeMarketStream();

    binance::Api restApi;
    std::optional<Stream> spotUserStream;
    std::optional<Stream> marketStream;
};


} // namespace tradebot
} // namespace ad
