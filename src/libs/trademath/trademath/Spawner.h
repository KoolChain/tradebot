#pragma once


#include "Ladder.h"

#include <boost/serialization/strong_typedef.hpp>


namespace ad {
namespace trade {


BOOST_STRONG_TYPEDEF(Decimal, Base);
BOOST_STRONG_TYPEDEF(Decimal, Quote);


inline std::ostream & operator<<(std::ostream & aOut, Base aBase)
{
    return aOut << static_cast<Decimal>(aBase);
}


inline std::ostream & operator<<(std::ostream & aOut, Quote aQuote)
{
    return aOut << static_cast<Decimal>(aQuote);
}


struct Spawn
{
    Spawn(Decimal aRate, Base aAmount) :
        rate{aRate},
        base{aAmount}
    {}

    Spawn(Decimal aRate, Quote aAmount) :
        rate{aRate},
        base{static_cast<Decimal>(aAmount) / rate}
    {}

    Decimal rate;
    Decimal base;
};


inline bool operator==(const Spawn aLhs, const Spawn aRhs)
{
    return aLhs.rate == aRhs.rate
        && aLhs.base == aRhs.base;
}


inline Decimal accumulateAmount(Decimal aLhs, const Spawn & aRhs)
{
    return aLhs + aRhs.base;
}


template <class T_amount, class T_ladderIt, class T_function>
std::pair<std::vector<Spawn>, T_amount/*accumulation*/>
spawnIntegration(const T_amount aAmount,
                 T_ladderIt aStopBegin, const T_ladderIt aStopEnd,
                 T_function aFunction)
{
    if (std::distance(aStopBegin, aStopEnd) < 1)
    {
        return {};
    }

    std::vector<Spawn> result;
    Decimal accumulation{0};
    for(T_ladderIt nextStop = aStopBegin+1;
        nextStop != aStopEnd;
        ++aStopBegin, ++nextStop)
    {
        Decimal amount = Base{abs(aFunction.integrate(*aStopBegin, *nextStop))};
        accumulation += amount;
        result.emplace_back(*nextStop, T_amount{amount});
    }
    return {result, T_amount{accumulation}};
}


template <class T_amount, class T_ladderIt, class T_proportionsIt>
std::pair<std::vector<Spawn>, T_amount/*accumulation*/>
spawnProportions(const T_amount aAmount,
                 T_ladderIt aStopBegin, const T_ladderIt aStopEnd,
                 T_proportionsIt aProportionsBegin, const T_proportionsIt aProportionsEnd)
{
    std::vector<Spawn> result;
    Decimal accumulation{0};
    while(aStopBegin != aStopEnd && aProportionsBegin != aProportionsEnd)
    {
        Decimal amount = (Decimal)aAmount*(*aProportionsBegin);
        accumulation += amount;
        result.emplace_back(*aStopBegin, T_amount{amount});
        ++aStopBegin;
        ++aProportionsBegin;
    }
    return {result, T_amount{accumulation}};
}


} // namespace trade
} // namespace ad
