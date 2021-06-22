#pragma once

#include "Decimal.h"


namespace ad {
namespace trade {


// Set to the maximal precision requested by Decimal type.
static const Decimal gDefaultTickSize{Decimal{1} / pow(10, EXCHANGE_DECIMALS)};

using TickSizeFiltered = std::pair<Decimal /*result*/, Decimal /*remainder*/>;


inline Decimal applyTickSize(Decimal aValue, Decimal aTickSize = gDefaultTickSize)
{
    auto count = trunc(aValue/aTickSize);
    return {aTickSize * count};
}


inline TickSizeFiltered computeTickFilter(Decimal aValue, Decimal aTickSize = gDefaultTickSize)
{
    auto filtered = applyTickSize(aValue, aTickSize);
    return {filtered, aValue - filtered};
}


} // namespace trade
} // namespace ad
