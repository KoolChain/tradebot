#pragma once

#include "Fragment.h"
#include "Fulfillment.h"

#include <binance/Orders.h>
#include <binance/Time.h>

#include <string>


namespace ad {
namespace tradebot {


struct Pair
{
    Coin base;
    Coin quote;

    binance::Symbol symbol() const
    {
        return base + quote;
    }
};


struct Order
{
    std::string symbol() const;
    Pair pair() const;
    binance::ClientId clientId() const;
    Decimal executionQuoteAmount() const;

    enum class Status
    {
        Sending,
        Cancelling,
        Inactive,
        Active,
        Fulfilled,
    };

    std::string traderName;
    Coin base;
    Coin quote;
    // TODO: Ideally, we want to be able to set the amout to be in base or quote (see binance::OrderBase)
    // But at the moment FOK orders only accept quantity in base, so we did not bother.
    Decimal baseAmount; // Quantity of base to exchange
    Decimal fragmentsRate; // This order was spawned from 1..n fragments at this rate.
                           // IMPORTANT: This is not necessarily the price at which this order will be issued!
    Side side;

    MillisecondsSinceEpoch activationTime{0};
    Status status{Status::Inactive};
    MillisecondsSinceEpoch fulfillTime{0};
    Decimal executionRate{0};
    Decimal commission{0};
    Coin commissionAsset;
    long exchangeId{-1}; // assigned by Binance
    long id{-1}; // auto-increment by ORM

    // Syntax convenience
    Order & setStatus(Status aStatus)
    {
        status = aStatus;
        return *this;
    }

    Order & reverseSide()
    {
        side = reverse(side);
        return *this;
    }

    // \brief Only keeps the client controlled values, effectively preparing a new inactive order
    // from the client values.
    Order & resetAsInactive();

    std::string getIdentity() const;
};


/// \brief Strictly equivalent to an Order, used to make sure some function receive an
/// Order with the "fulfill-related" fields completed.
struct FulfilledOrder : public Order
{};


FulfilledOrder fulfill(Order & aOrder,
                       const Json & aQueryStatus,
                       const Fulfillment & aFulfillment);


inline binance::MarketOrder to_marketOrder(const Order & aOrder)
{
    return {
        aOrder.symbol(),
        (aOrder.side == Side::Sell ? binance::Side::SELL : binance::Side::BUY),
        aOrder.baseAmount,
        binance::QuantityUnit::Base,
        aOrder.clientId(),
    };
}


inline binance::LimitOrder to_limitOrder(const Order & aOrder, Decimal aLimitRate)
{
    return {
        binance::OrderBase{
            aOrder.symbol(),
            (aOrder.side == Side::Sell ? binance::Side::SELL : binance::Side::BUY),
            aOrder.baseAmount,
            binance::QuantityUnit::Base,
            aOrder.clientId(),
        },
        aLimitRate,
    };
}

inline binance::LimitOrder to_limitFokOrder(const Order & aOrder, Decimal aLimitRate)
{
    binance::LimitOrder order = to_limitOrder(aOrder, aLimitRate);
    order.timeInForce = binance::TimeInForce::FOK;
    return order;
}


/// \important Compares every data member except for the `id`!
bool operator==(const Order & aLhs, const Order & aRhs);


bool operator!=(const Order & aLhs, const Order & aRhs);


std::ostream & operator<<(std::ostream & aOut, const Order & aRhs);


inline std::ostream & operator<<(std::ostream & aOut, const Order::Status & aStatus)
{
    return aOut << std::string{
        [](const Order::Status & aStatus)
        {
            switch(aStatus)
            {
                case Order::Status::Sending:
                    return "Sending";
                case Order::Status::Cancelling:
                    return "Cancelling";
                case Order::Status::Inactive:
                    return "Inactive";
                case Order::Status::Active:
                    return "Active";
                case Order::Status::Fulfilled:
                    return "Fulfilled";
                default:
                    throw std::domain_error{"Invalid Status enumerator."};
            }
        }(aStatus)};
}


} // namespace tradebot
} // namespace ad
