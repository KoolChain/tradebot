#pragma once

#include "testnet_secrets.h"

#include <tradebot/Database.h>
#include <tradebot/Exchange.h>
#include <tradebot/Order.h>
#include <tradebot/Trader.h>


namespace ad {


inline tradebot::Order makeOrder(const std::string & aTraderName,
                                 const tradebot::Pair & aPair,
                                 tradebot::Side aSide,
                                 Decimal aPrice = Decimal{"0"},
                                 Decimal aAmount = Decimal{"0.001"})
{
    return tradebot::Order{
        aTraderName,
        aPair.base,
        aPair.quote,
        aAmount,
        aPrice,
        aSide,
        tradebot::Order::FulfillResponse::SmallSpread,
    };
}

inline tradebot::Order makeImpossibleOrder(tradebot::Exchange & aExchange,
                                           const std::string & aTraderName,
                                           const tradebot::Pair & aPair,
                                           tradebot::Side aSide = tradebot::Side::Sell, // if it still fulfills, might as well make us rich!
                                           Decimal aAmount = Decimal{"0.001"})
{
    Decimal averagePrice = aExchange.getCurrentAveragePrice(aPair);
    return makeOrder(aTraderName, aPair, aSide, floor(averagePrice*4), aAmount);
}


inline tradebot::Trader makeTrader(const std::string & aTraderName,
                                   tradebot::Pair aPair = tradebot::Pair{"BTC", "USDT"})
{
    return tradebot::Trader{
        aTraderName,
        aPair,
        tradebot::Database{":memory:"},
        tradebot::Exchange{binance::Api{secret::gTestnetCredentials, binance::Api::gTestNet}}
    };
}


inline tradebot::FulfilledOrder mockupFulfill(tradebot::Order & aOrder, Decimal aExecutionRate)
{
    aOrder.status = tradebot::Order::Status::Fulfilled;
    aOrder.fulfillTime = getTimestamp();
    aOrder.executionRate = aExecutionRate;

    return {aOrder};
}


inline std::pair<tradebot::Order, std::vector<tradebot::Fragment>>
makeOrderWithFragments(
        tradebot::Database & aDatabase,
        const std::string aTraderName,
        tradebot::Pair aPair,
        std::initializer_list<Decimal> aFragmentAmounts,
        Decimal aRate,
        tradebot::Side aSide,
        long aSpawningOrderId = -1)
{
    std::vector<tradebot::Fragment> fragments;
    for (const auto amount : aFragmentAmounts)
    {
        fragments.push_back(
            tradebot::Fragment{
                aPair.base,
                aPair.quote,
                amount,
                aRate,
                aSide,
                0, // taken home
                aSpawningOrderId
        });

        aDatabase.insert(fragments.back());
    }

    tradebot::Order order = aDatabase.prepareOrder(
            aTraderName,
            aSide,
            aRate,
            aPair,
            tradebot::Order::FulfillResponse::SmallSpread);

    for (auto & fragment : fragments)
    {
        aDatabase.reload(fragment);
    }

    return {order, std::move(fragments)};
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


#define REQUIRE_FILLED_MARKET_ORDER(order, orderSide, mTraderName, pair, averagePrice, timeBefore) \
    REQUIRE(order.status == Order::Status::Fulfilled);  \
    REQUIRE(order.traderName == (mTraderName));         \
    REQUIRE(order.symbol() == pair.symbol());           \
    REQUIRE(order.executionRate > averagePrice * 0.5);  \
    REQUIRE(order.executionRate < averagePrice * 1.5);  \
    REQUIRE(order.side == orderSide);                   \
    REQUIRE(order.activationTime >= timeBefore);        \
    REQUIRE(order.fulfillTime >= order.activationTime); \
    REQUIRE(order.fulfillTime <= getTimestamp());       \
    REQUIRE(order.exchangeId != -1);

} // namespace ad
