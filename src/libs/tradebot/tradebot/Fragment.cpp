#include "Fragment.h"

#include <ostream>


namespace ad {
namespace tradebot {


bool Fragment::isInitial() const
{
    return spawningOrder == -1;
}


std::string Fragment::getIdentity() const
{
    return std::to_string(id);
}

bool operator==(const Fragment & aLhs, const Fragment & aRhs)
{
    return
        aLhs.base == aRhs.base
        && aLhs.quote == aRhs.quote
        && isEqual(aLhs.baseAmount, aRhs.baseAmount)
        && isEqual(aLhs.targetRate, aRhs.targetRate)
        && aLhs.side == aRhs.side
        && isEqual(aLhs.takenHome, aRhs.takenHome)
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
        << aRhs.baseAmount << ' ' << aRhs.base
        << " (@" << aRhs.targetRate << " " << aRhs.quote << " per " << aRhs.base << ")"
        << " [parent: " << aRhs.spawningOrder << ", composes: " << aRhs.composedOrder << "]"
        ;
}


} // namespace tradebot
} // namespace ad
