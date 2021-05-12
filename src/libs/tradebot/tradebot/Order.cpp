#include "Order.h"

#include <ostream>


namespace ad {
namespace tradebot {


std::string Order::symbol() const
{
    return base + quote;
}


std::string Order::clientId(const std::string & aTraderName) const
{
    return aTraderName + std::to_string(id);
}


bool operator==(const Order & aLhs, const Order & aRhs)
{
    return
        aLhs.base == aRhs.base
        && aLhs.quote == aRhs.quote
        && aLhs.amount == aRhs.amount
        && aLhs.fragmentsRate == aRhs.fragmentsRate
        && aLhs.executionRate== aRhs.executionRate
        && aLhs.direction == aRhs.direction
        && aLhs.fulfillResponse == aRhs.fulfillResponse
        && aLhs.creationTime == aRhs.creationTime
        && aLhs.status == aRhs.status
        && aLhs.takenHome == aRhs.takenHome
        && aLhs.fulfillTime == aRhs.fulfillTime
        && aLhs.exchangeId == aRhs.exchangeId
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
