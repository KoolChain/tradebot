#pragma once

#include "Fragment.h"

#include <string>


namespace ad {
namespace tradebot {


struct Pair
{
    std::string base;
    std::string quote;
};


struct Order
{
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

    std::string base;
    std::string quote;
    Decimal amount;
    Decimal fragmentsRate;
    Decimal executionRate;
    Direction direction;

    FulfillResponse fulfillResponse;

    MillisecondsSinceEpoch creationTime{0};
    Status status{Status::Inactive};
    Decimal takenHome{0};
    MillisecondsSinceEpoch fulfillTime{0};
    long exchangeId{-1}; // assigned by Binance
    long id{-1}; // auto-increment by ORM
};


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
