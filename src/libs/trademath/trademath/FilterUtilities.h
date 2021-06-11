#pragma once

#include "Decimal.h"


namespace ad {
namespace trade {


static const Decimal gDefaultTicksize{"0.001"};


inline Decimal applyTickSize(Decimal aValue, Decimal aTickSize = gDefaultTicksize)
{
    auto count = trunc(aValue/aTickSize);
    return aTickSize*count;
}


} // namespace trade
} // namespace ad
