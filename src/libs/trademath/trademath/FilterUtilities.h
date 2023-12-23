#pragma once

#include "Decimal.h"


namespace ad {
namespace trade {


// Set to the maximal precision requested by Decimal type.
static const Decimal gDefaultTickSize{Decimal{1} / pow(10, EXCHANGE_DECIMALS)};

using TickSizeFiltered = std::pair<Decimal /*result*/, Decimal /*remainder*/>;


inline Decimal applyTickSizeFloor(Decimal aValue, Decimal aTickSize = gDefaultTickSize)
{
    if(aTickSize == 0)
    {
        return aValue;
    }
    else
    {
        auto count = trunc(aValue/aTickSize);
        return {aTickSize * count};
    }
}


inline Decimal applyTickSizeCeil(Decimal aValue, Decimal aTickSize)
{
    Decimal filtered = applyTickSizeFloor(aValue, aTickSize);
    if(filtered < aValue)
    {
        filtered += aTickSize;
    }
    return filtered;
}


/// @return A pair of Decimal: {filtered result, remainder}.
inline TickSizeFiltered computeTickFilter(Decimal aValue, Decimal aTickSize = gDefaultTickSize)
{
    auto filtered = applyTickSizeFloor(aValue, aTickSize);
    return {filtered, aValue - filtered};
}


} // namespace trade
} // namespace ad
