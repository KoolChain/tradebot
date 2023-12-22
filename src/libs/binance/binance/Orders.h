#pragma once


#include "Json.h"

#include <trademath/Decimal.h>

#include <string>


namespace ad {
namespace binance {


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
        {
            static const std::string buy{"BUY"};
            return buy;
        }
        case Side::SELL:
        {
            static const std::string sell{"SELL"};
            return sell;
        }
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
        {
            static const std::string market{"MARKET"};
            return market;
        }
        case Type::LIMIT:
        {
            static const std::string limit{"LIMIT"};
            return limit;
        }
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


/// \brief Not default constructible, explicitly typed.
class ClientId
{
public:
    explicit ClientId(std::string aId) :
        mId{std::move(aId)}
    {}

    explicit operator const std::string &() const
    {
        return mId;
    }

    bool operator==(const ClientId & aRhs) const
    {
        return mId == aRhs.mId;
    }

    bool operator!=(const ClientId & aRhs) const
    {
        return ! (*this == aRhs);
    }

private:
    std::string mId;
};


enum class QuantityUnit
{
    Base,
    Quote,
};


struct OrderBase
{
    Symbol symbol;
    Side side;
    Decimal quantity;
    QuantityUnit unit = QuantityUnit::Base;
    ClientId clientId;
};


struct MarketOrder : public OrderBase
{
    const Type type{Type::MARKET};
};


struct LimitOrder : public OrderBase
{
    Decimal price;
    TimeInForce timeInForce{TimeInForce::GTC};
    const Type type{Type::LIMIT};
};

} // namespace binance
} // namespace ad
