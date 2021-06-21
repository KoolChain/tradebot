#pragma once

#include "Decimal.h"


namespace ad {
namespace trade {


static const Decimal gDefaultTicksize{"0.001"};


using TickSizeFiltered = std::pair<Decimal /*result*/, Decimal /*remainder*/>;


inline Decimal applyTickSize(Decimal aValue, Decimal aTickSize = gDefaultTicksize)
{
    auto count = trunc(aValue/aTickSize);
    return {aTickSize * count};
}


inline TickSizeFiltered computeTickFilter(Decimal aValue, Decimal aTickSize = gDefaultTicksize)
{
    auto filtered = applyTickSize(aValue, aTickSize);
    return {filtered, aValue - filtered};
}


} // namespace trade
} // namespace ad
