#pragma once

#include "../Fragment.h"

#include "../Logging.h"

#include <trademath/Ladder.h>


namespace ad {
namespace tradebot {
namespace spawner {


// Note: Duplicates code from Ladder.h, but allows for more specific logging in case of error.
template <class T_ladderIterator>
T_ladderIterator getStopFor(T_ladderIterator aBegin, const T_ladderIterator aEnd,
                            Fragment aFragment)
{
    auto stop = std::find(aBegin, aEnd, aFragment.targetRate);
    if (stop == aEnd)
    {
        spdlog::critical("Filled fragment cannot match to a ladder stop: '{}'.",
                         boost::lexical_cast<std::string>(aFragment));
        throw std::logic_error{"Filled fragment does not match a ladder stop."};
    }
    return stop;
}


} // namespace spawner
} // namespace tradebot
} // namespace ad
