#pragma once


#include "Ladder.h"

#include <boost/serialization/strong_typedef.hpp>

#include <algorithm>


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

    template <class T_amount>
    Decimal getAmount() const;

    Decimal rate;
    Decimal base;
};


template <>
inline Decimal Spawn::getAmount<Base>() const
{
    return base;
}


template <>
inline Decimal Spawn::getAmount<Quote>() const
{
    return base * rate;
}


inline bool operator==(const Spawn aLhs, const Spawn aRhs)
{
    return aLhs.rate == aRhs.rate
        && aLhs.base == aRhs.base;
}


inline bool operator!=(const Spawn aLhs, const Spawn aRhs)
{
    return ! (aLhs == aRhs);
}


/// \brief Helper function to accumulate the base or quote amounts of a range of Spawn instances.
template <class T_amount>
inline Decimal accumulateAmount(Decimal aLhs, const Spawn & aRhs)
{
    return aLhs + aRhs.getAmount<T_amount>();
}


template <class T_spawnRange>
inline Decimal sumSpawnBase(const T_spawnRange & aSpawn)
{
    return std::accumulate(aSpawn.begin(), aSpawn.end(),
                           Decimal{0},
                           accumulateAmount<Base>);
}

template <class T_spawnRange>
inline Decimal sumSpawnQuote(const T_spawnRange & aSpawn)
{
    return std::accumulate(aSpawn.begin(), aSpawn.end(),
                           Decimal{0},
                           accumulateAmount<Quote>);
}


template <class T_amount>
using SpawnResult = std::pair<std::vector<Spawn>, T_amount/*accumulation*/>;

/// \brief Compute a vector of `Spawn` and the corresponding accumulated base amount from a proportion function.
/// Can apply a tick size, filtering in `Base` values.
///
/// It integrates the proportion function `aFunction` over the ladder intervals,
/// and apply the proportion to `aAmount` at each interval, assigning the resulting amount to the
/// interval farthest stop.
///
/// \note For normal iterators, farthest stop is the upper value in the interval,
/// while for reverse iterators it the the lower value.
template <class T_amount, class T_ladderIt, class T_function>
SpawnResult<T_amount>
spawnIntegration(const T_amount aAmount,
                 T_ladderIt aStopBegin, const T_ladderIt aStopEnd,
                 T_function aFunction,
                 Decimal aTickSize = Decimal{0})
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
        Spawn spawn{*nextStop, T_amount{amount}};
        if (aTickSize != 0) // we would expect compilers to optimize that away on default value
        {
            // Always apply the tick size to base value.
            // (For the moment the libraries only allow placing order by giving the base value).
            spawn.base = applyTickSize(spawn.base, aTickSize);
        }
        accumulation += spawn.getAmount<T_amount>();
        result.push_back(std::move(spawn));
    }
    return {result, T_amount{accumulation}};
}


/// \brief Compute a vector of `Spawn` and the corresponding accumulated base amount from a proportions' range.
/// Can apply a tick size, filtering in `Base` values.
///
/// Applies the proportions in the proportions' range to `aAmount`,
/// assigning the results to the stops in the ladder range.
template <class T_amount, class T_ladderIt, class T_proportionsIt>
SpawnResult<T_amount>
spawnProportions(const T_amount aAmount,
                 T_ladderIt aStopBegin, const T_ladderIt aStopEnd,
                 T_proportionsIt aProportionsBegin, const T_proportionsIt aProportionsEnd,
                 Decimal aTickSize = Decimal{0})
{
    std::vector<Spawn> result;
    Decimal accumulation{0};
    while(aStopBegin != aStopEnd && aProportionsBegin != aProportionsEnd)
    {
        Decimal amount = (Decimal)aAmount * (*aProportionsBegin);
        Spawn spawn{*aStopBegin, T_amount{amount}};
        if (aTickSize != 0) // we would expect compilers to optimize that away on default value
        {
            // Always apply the tick size to base value
            // (For the moment the libraries only allow placing order by giving the base value).
            spawn.base = applyTickSize(spawn.base, aTickSize);
        }
        accumulation += spawn.getAmount<T_amount>();
        result.push_back(std::move(spawn));
        ++aStopBegin;
        ++aProportionsBegin;
    }
    return {result, T_amount{accumulation}};
}


} // namespace trade
} // namespace ad
