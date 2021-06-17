#pragma once

#include "Decimal.h"
#include "FilterUtilities.h"

#include <vector>


namespace ad {
namespace trade {


using Ladder = std::vector<Decimal>;


Ladder makeLadder(Decimal aFirstRate,
                  Decimal aFactor,
                  std::size_t aStopCount,
                  Decimal aTickSize = gDefaultTicksize);


// Only exist so getStopFor can log without exposing the spdlog header here.
void logCritical(const std::string aMessage);


template <class T_ladderIterator>
T_ladderIterator getStopFor(T_ladderIterator aBegin, const T_ladderIterator aEnd,
                            Decimal aTargetRate)
{
    auto stop = std::find(aBegin, aEnd, aTargetRate);
    if (stop == aEnd)
    {
        logCritical("Cannot match a ladder stop to rate: '"
                    + boost::lexical_cast<std::string>(aTargetRate)
                    + "'.");
        throw std::logic_error{"Target rate does not match a ladder stop."};
    }
    return stop;
}


} // namespace trade
} // namespace ad
