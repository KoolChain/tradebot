#pragma once


#include "Json.h"

#include <string>


namespace ad {
namespace binance {


// TODO use a real decimal type
using Decimal = double;


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
}


enum class Type
{
    MARKET,
};


inline const std::string & to_string(Type aType)
{
    switch (aType)
    {
        case Type::MARKET:
            static const std::string market{"MARKET"};
            return market;
    }
}


struct Order
{
    virtual Json json() const = 0;
};


struct MarketOrder : public Order
{
    std::string symbol;
    Side side;
    const Type type{Type::MARKET};
    Decimal quantity;

    Json json() const override
    {
        return {
            {"symbol", symbol},
            {"side", to_string(side)},
            {"type", to_string(type)},
            {"quantity", std::to_string(quantity)},
        };
    }
};


} // namespace binance
} // namespace ad
