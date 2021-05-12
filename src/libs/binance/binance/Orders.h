#pragma once


#include "Json.h"

#include <string>


namespace ad {
namespace binance {


// TODO use a real decimal type
using Decimal = double;


// No intention to specialize that right now
// but this way it will already be in place
using Symbol = std::string;


enum class Side
{
    BUY,
    SELL,
};


inline const std::string & to_string(Side aSide)
{
    switch (aSide)
    {
        case Side::BUY:
            static const std::string buy{"BUY"};
            return buy;
        case Side::SELL:
            static const std::string sell{"SELL"};
            return sell;
    }
    throw std::invalid_argument{"Unexpected Side: " + std::to_string(static_cast<int>(aSide))};
}


enum class Type
{
    MARKET,
    LIMIT,
};


inline const std::string & to_string(Type aType)
{
    switch (aType)
    {
        case Type::MARKET:
            static const std::string market{"MARKET"};
            return market;
        case Type::LIMIT:
            static const std::string limit{"LIMIT"};
            return limit;
    }
    throw std::invalid_argument{"Unexpected Type: " + std::to_string(static_cast<int>(aType))};
}


enum class TimeInForce
{
    GTC,
    IOC,
    FOK,
    // Note: We would really like AON, but Binance API does not seem to offer it.
};


inline const std::string & to_string(TimeInForce aTimeInForce)
{
    switch (aTimeInForce)
    {
        case TimeInForce::GTC:
        {
            static const std::string result{"GTC"};
            return result;
        }
        case TimeInForce::IOC:
        {
            static const std::string result{"IOC"};
            return result;
        }
        case TimeInForce::FOK:
        {
            static const std::string result{"FOK"};
            return result;
        }
    }
    throw std::invalid_argument{"Unexpected TimeInForce: "
                                + std::to_string(static_cast<int>(aTimeInForce))};
}


//struct Order
//{
//    virtual ~Order() = default;
//    virtual Json json() const = 0;
//};


struct MarketOrder
{
    Symbol symbol;
    Side side;
    Decimal quantity;
    const Type type{Type::MARKET};

    //Json json() const override
    //{
    //    return {
    //        {"symbol", symbol},
    //        {"side", to_string(side)},
    //        {"type", to_string(type)},
    //        {"quantity", std::to_string(quantity)},
    //    };
    //}
};

struct LimitOrder
{
    Symbol symbol;
    Side side;
    Decimal quantity;
    Decimal price;
    TimeInForce timeInForce{TimeInForce::GTC};
    const Type type{Type::LIMIT};

    //Json json() const override
    //{
    //    return {
    //        {"symbol", symbol},
    //        {"side", to_string(side)},
    //        {"type", to_string(type)},
    //        {"quantity", std::to_string(quantity)},
    //    };
    //}
};

} // namespace binance
} // namespace ad
