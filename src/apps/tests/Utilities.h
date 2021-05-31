#pragma once

#include <tradebot/Order.h>


namespace ad {


inline tradebot::Order makeOrder(const std::string aTraderName,
                                 const tradebot::Pair & aPair,
                                 tradebot::Side aSide,
                                 binance::Decimal aPrice = 0.,
                                 binance::Decimal aAmount = 0.001)
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


} // namespace ad
