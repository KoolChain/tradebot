#include "Order.h"

#include <ostream>


namespace ad {
namespace tradebot {


bool operator==(const Order & aLhs, const Order & aRhs)
{
    return
        aLhs.base == aRhs.base
        && aLhs.quote == aRhs.quote
        && aLhs.amount == aRhs.amount
        && aLhs.fragmentsRate == aRhs.fragmentsRate
        && aLhs.executionRate== aRhs.executionRate
        && aLhs.direction == aRhs.direction
        && aLhs.creationTime == aRhs.creationTime
        && aLhs.fulfillResponse == aRhs.fulfillResponse
        && aLhs.status == aRhs.status
        && aLhs.takenHome == aRhs.takenHome
        && aLhs.fulfillTime == aRhs.fulfillTime
        ;
}


bool operator!=(const Order & aLhs, const Order & aRhs)
{
    return ! (aLhs == aRhs);
}


std::ostream & operator<<(std::ostream & aOut, const Order & aRhs)
{
    return aOut
        << aRhs.status << " order " << std::string(aRhs.direction == Direction::Sell ? "Sell" : "Buy")
        << ' ' << aRhs.amount << ' ' << aRhs.base
        << " (fragments: " << aRhs.fragmentsRate
        << " execution: " << aRhs.executionRate << ' '
        << aRhs.quote << " per " << aRhs.base << ")"
        ;
}


} // namespace tradebot
} // namespace ad
