#pragma once


#include "Order.h"

#include <binance/Api.h>


namespace ad {
namespace tradebot {


enum class Execution
{
    Market,
    Limit,
};

struct Exchange
{
    /// \return "NOTEXISTING" if the exchange engine does not know the order, otherwise returns
    ///         the value from the exchange:
    ///         * NEW (Active order)
    ///         * CANCELED
    ///         * FILLED
    std::string getOrderStatus(const Order & aOrder, const std::string & aTraderName);

    Decimal getCurrentAveragePrice(const Pair & aPair);

    Order placeOrder(Order & aOrder, Execution aExecution, const std::string & aTraderName);

    /// \return true if the order was cancelled, false otherwise
    ///         (because it was not present, already cancelled, etc.).
    bool cancelOrder(const Order & aOrder, const std::string & aTraderName);

    std::vector<binance::ClientId> cancelAllOpenOrders(const Pair & aPair);

    std::vector<binance::ClientId> listOpenOrders(const Pair & aPair);

    binance::Api restApi;
};


} // namespace tradebot
} // namespace ad
