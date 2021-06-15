#pragma once


#include "Ladder.h"

#include <boost/serialization/strong_typedef.hpp>


namespace ad {
namespace trade {


// Note: this is a shitshow: these strong typedefs prevent usefull arithmetic between Base(or Quote) and Decimal
// but allow dangerous `==` comparison between Base and Quote...
// We were able to patch the comparison with deletions below.
// TODO find something better with expected behaviour.
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


bool operator==(const Base &, const Quote &) = delete;
bool operator==(const Quote &, const Base &) = delete;
bool operator!=(const Base &, const Quote &) = delete;
bool operator!=(const Quote &, const Base &) = delete;


/// \brief A named pair, associating a **base** amount to a rate
///
/// The rate can be used to represent the limit at which the base amount is expected to be traded.
/// In a conservative implementation:
/// * lower limit in case of a `Sell`
/// * upper limit in case of a `Buy`
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


inline bool operator!=(const Spawn aLhs, const Spawn aRhs)
{
    return ! (aLhs == aRhs);
}


/// \brief Helper function to accumulate the base amounts of a range of Spawn instances.
inline Decimal accumulateBaseAmount(Decimal aLhs, const Spawn & aRhs)
{
    return aLhs + aRhs.base;
}


template <class T_spawnRange>
inline Decimal sumSpawnBase(const T_spawnRange & aSpawn)
{
    return std::accumulate(aSpawn.begin(), aSpawn.end(),
                           Decimal{0},
                           accumulateBaseAmount);
}


/// \brief Helper function to accumulate the quote amounts of a range of Spawn instances.
inline Decimal accumulateQuoteAmount(Decimal aLhs, const Spawn & aRhs)
{
    return aLhs + aRhs.base*aRhs.rate;
}


template <class T_spawnRange>
inline Decimal sumSpawnQuote(const T_spawnRange & aSpawn)
{
    return std::accumulate(aSpawn.begin(), aSpawn.end(),
                           Decimal{0},
                           accumulateQuoteAmount);
}


/// \brief Compute a vector of `Spawn` and the corresponding accumulated base amount from a proportion function.
///
/// It integrates the proportion function `aFunction` over the ladder intervals,
/// and apply the proportion to `aAmount` at each interval, assigning the resulting amount to the
/// interval farthest stop.
///
/// \note For normal iterators, farthest stop is the upper value in the interval,
/// while for reverse iterators it the the lower value.
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
        Decimal amount = (Decimal)aAmount * abs(aFunction.integrate(*aStopBegin, *nextStop));
        accumulation += amount;
        result.emplace_back(*nextStop, T_amount{amount});
    }
    return {result, T_amount{accumulation}};
}


/// \brief Compute a vector of `Spawn` and the corresponding accumulated base amount from a proportions' range.
///
/// Applies the proportions in the proportions' range to `aAmount`,
/// assigning the results to the stops in the ladder range.
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