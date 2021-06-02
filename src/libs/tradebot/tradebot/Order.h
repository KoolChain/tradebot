#pragma once

#include "Fragment.h"
#include "Fulfillment.h"

#include <binance/Orders.h>
#include <binance/Time.h>

#include <string>


namespace ad {
namespace tradebot {


using Coin = std::string;


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
    binance::ClientId clientId() const;

    enum class Status
    {
        Sending,
        Cancelling,
        Inactive,
        Active,
        Fulfilled,
    };

    enum class FulfillResponse
    {
        SmallSpread,
    };

    std::string traderName;
    Coin base;
    Coin quote;
    Decimal amount; // Quantity of base to exchange
    Decimal fragmentsRate;
    Side side;

    FulfillResponse fulfillResponse;

    MillisecondsSinceEpoch activationTime{0};
    Status status{Status::Inactive};
    MillisecondsSinceEpoch fulfillTime{0};
    Decimal executionRate{0};
    Decimal commission{0};
    Coin commissionAsset;
    Decimal takenHome{0};
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
        side = (side == Side::Sell ? Side::Buy : Side::Sell);
        return *this;
    }

    std::string getIdentity() const;
};


/// \brief Strictly equivalent to an Order, used to make sure some function receive an
/// Order with the "fulfill-related" fields completed.
struct FulfilledOrder : public Order
{};


FulfilledOrder fulfill(const Order & aOrder,
                       const Json & aQueryStatus,
                       const Fulfillment & aFulfillment);


inline binance::MarketOrder to_marketOrder(const Order & aOrder)
{
    return {
        aOrder.symbol(),
        (aOrder.side == Side::Sell ? binance::Side::SELL : binance::Side::BUY),
        aOrder.amount,
        aOrder.clientId(),
    };
}


inline binance::LimitOrder to_limitOrder(const Order & aOrder)
{
    return {
        aOrder.symbol(),
        (aOrder.side == Side::Sell ? binance::Side::SELL : binance::Side::BUY),
        aOrder.amount,
        aOrder.clientId(),
        aOrder.fragmentsRate,
    };
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
            }
        }(aStatus)};
}


} // namespace tradebot
} // namespace ad
