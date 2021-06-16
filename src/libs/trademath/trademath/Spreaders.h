#pragma once


#include "Ladder.h"
#include "Spawn.h"


namespace ad {
namespace trade {


/// The Spreader concept encapsulate a spawning function and its specific associated data
/// behind a consistent interface that can be used as template argument.
/// This is one abstraction allowing for more generic spawners.


struct ProportionSpreader
{
    template <class T_amount>
    SpawnResult<T_amount> spreadDown(T_amount aAmount, Decimal aFromRate);
    template <class T_amount>
    SpawnResult<T_amount> spreadUp(T_amount aAmount, Decimal aFromRate);

    Ladder ladder;
    std::vector<Decimal> proportions;
};


template <class T_amount>
SpawnResult<T_amount> ProportionSpreader::spreadDown(T_amount aAmount, Decimal aFromRate)
{
    // reverseStop will never be ladder.rend(): getStopFor() throws when stop is not found.
    auto reverseStop = getStopFor(ladder.crbegin(), ladder.crend(), aFromRate);

    return trade::spawnProportions(aAmount,
                                   // +1 because no fragment should be assigned to the current stop.
                                   reverseStop+1, ladder.crend(),
                                   proportions.cbegin(), proportions.cend());
}


template <class T_amount>
SpawnResult<T_amount> ProportionSpreader::spreadUp(T_amount aAmount, Decimal aFromRate)
{
    // reverseStop will never be ladder.end(): getStopFor() throws when stop is not found.
    auto forwardStop = getStopFor(ladder.cbegin(), ladder.cend(), aFromRate);

    return trade::spawnProportions(aAmount,
                                   // +1 because no fragment should be assigned to the current stop.
                                   forwardStop+1, ladder.cend(),
                                   proportions.cbegin(), proportions.cend());
}


} // namespace trade
} // namespace ad
