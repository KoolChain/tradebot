#pragma once


#include "Ladder.h"
#include "Spawn.h"

#include <spdlog/spdlog.h>


namespace ad {
namespace trade {


/// The Spreader concept encapsulate a spawning function and its specific associated data
/// behind a consistent interface that can be used as template argument.
/// This is one abstraction allowing for more generic spawners.

using ProportionsMap = std::vector<std::pair<Decimal/*max rate*/, std::vector<Decimal>/*proportions*/>>;

struct ProportionSpreader
{
    template <class T_amount>
    SpawnResult<T_amount> spreadDown(T_amount aAmount, Decimal aFromRate);
    template <class T_amount>
    SpawnResult<T_amount> spreadUp(T_amount aAmount, Decimal aFromRate);

    const std::vector<Decimal> & getProportions(Decimal aFromRate) const;

    Ladder ladder;
    ProportionsMap maxRateToProportions;
    Decimal amountTickSize{trade::gDefaultTickSize};
};


inline const std::vector<Decimal> & ProportionSpreader::getProportions(Decimal aFromRate) const
{
    for(const auto & [maxRate, proportions] : maxRateToProportions)
    {
        if(aFromRate <= maxRate)
        {
            return proportions;
        }
    }
    spdlog::error("Rate {} is above the last proportions max rate {}. Using last proportions.",
                  static_cast<float>(aFromRate),
                  static_cast<float>(maxRateToProportions.back().first));
    return maxRateToProportions.back().second;
}


template <class T_amount>
SpawnResult<T_amount> ProportionSpreader::spreadDown(T_amount aAmount, Decimal aFromRate)
{
    // reverseStop will never be ladder.rend(): getStopFor() throws when stop is not found.
    auto reverseStop = getStopFor(ladder.crbegin(), ladder.crend(), aFromRate);

    auto proportions = getProportions(aFromRate);
    return trade::spawnProportions(aAmount,
                                   // +1 because no fragment should be assigned to the current stop.
                                   reverseStop+1, ladder.crend(),
                                   proportions.cbegin(), proportions.cend(),
                                   amountTickSize);
}


template <class T_amount>
SpawnResult<T_amount> ProportionSpreader::spreadUp(T_amount aAmount, Decimal aFromRate)
{
    // reverseStop will never be ladder.end(): getStopFor() throws when stop is not found.
    auto forwardStop = getStopFor(ladder.cbegin(), ladder.cend(), aFromRate);

    return trade::spawnProportions(aAmount,
                                   // +1 because no fragment should be assigned to the current stop.
                                   forwardStop+1, ladder.cend(),
                                   proportions.cbegin(), proportions.cend(),
                                   amountTickSize);
}


} // namespace trade
} // namespace ad
