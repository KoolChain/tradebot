#include "Fragment.h"

#include <ostream>


namespace ad {
namespace tradebot {


bool operator==(const Fragment & aLhs, const Fragment & aRhs)
{
    return
        aLhs.base == aRhs.base
        && aLhs.quote == aRhs.quote
        && aLhs.amount == aRhs.amount
        && aLhs.targetRate == aRhs.targetRate
        && aLhs.side == aRhs.side
        && aLhs.spawningOrder == aRhs.spawningOrder
        && aLhs.composedOrder == aRhs.composedOrder
        ;
}

bool operator!=(const Fragment & aLhs, const Fragment & aRhs)
{
    return ! (aLhs == aRhs);
}

std::ostream & operator<<(std::ostream & aOut, const Fragment & aRhs)
{
    return aOut
        << "Fragment " << std::string{aRhs.side == Side::Sell ? "Sell" : "Buy"} << ' '
        << aRhs.amount << ' ' << aRhs.base
        << " (@" << aRhs.targetRate << " " << aRhs.quote << " per " << aRhs.base << ")"
        ;
}


} // namespace tradebot
} // namespace ad
