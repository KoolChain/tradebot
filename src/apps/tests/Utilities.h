#pragma once

#include <tradebot/Database.h>
#include <tradebot/Exchange.h>
#include <tradebot/Order.h>


namespace ad {


inline tradebot::Order makeOrder(const std::string aTraderName,
                                 const tradebot::Pair & aPair,
                                 tradebot::Side aSide,
                                 Decimal aPrice = 0.,
                                 Decimal aAmount = 0.001)
{
    return tradebot::Order{
        aTraderName,
        aPair.base,
        aPair.quote,
        aAmount,
        aPrice,
        0.,
        aSide,
        tradebot::Order::FulfillResponse::SmallSpread,
    };
}


template <class T_exchange>
inline void fulfillMarketOrder(T_exchange & aExchange, tradebot::Order & aOrder)
{
    aExchange.placeOrder(aOrder, tradebot::Execution::Market);
    std::string status;
    while((status = aExchange.getOrderStatus(aOrder)) == "EXPIRED")
    {
        aExchange.placeOrder(aOrder, tradebot::Execution::Market);
    }
    if (status != "FILLED")
    {
        throw std::runtime_error{"Market order returned status '" + status + "'."};
    }
}


template <class T_exchange>
inline void revertOrder(T_exchange & aExchange, tradebot::Database & aDatabase, tradebot::Order aOrder)
{
    aDatabase.insert(aOrder.reverseSide());
    aExchange.placeOrder(aOrder, tradebot::Execution::Market);

    while(aExchange.getOrderStatus(aOrder) == "EXPIRED")
    {
        aExchange.placeOrder(aOrder, tradebot::Execution::Market);
    }
}

} // namespace ad
