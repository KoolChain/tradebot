#pragma once


#include "Decimal.h"


#include <functional>


namespace ad {
namespace trade {


struct Function
{
    using Function_t = std::function<Decimal(Decimal)>;

    Decimal integrate(Decimal aLowValue, Decimal aHighValue);

    Function_t primitive;
};


inline Decimal Function::integrate(Decimal aLowValue, Decimal aHighValue)
{
    return primitive(aHighValue) - primitive(aLowValue);
}


} // namespace trade
} // namespace ad
