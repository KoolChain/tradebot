#include "Ladder.h"

#include "DecimalLog.h"

#include <algorithm>
#include <spdlog/spdlog.h>


namespace ad {
namespace trade {


Ladder makeLadder(Decimal aFirstRate,
                  Decimal aFactor,
                  std::size_t aStopCount,
                  Decimal aTickSize)
{
    if (aFactor <= 1)
    {
        spdlog::critical("Invalid increase {} in ladder generation, must be > 1.", aFactor);
        throw std::domain_error{"Invalid increase for ladder generation."};
    }

    if (aStopCount < 1)
    {
        spdlog::critical("Invalid number of stops {} in ladder generation, must be >= 1.", aStopCount);
        throw std::domain_error{"Invalid stop count for ladder generation."};
    }

    Decimal previous = applyTickSize(aFirstRate, aTickSize);
    Ladder result{previous};

    std::generate_n(std::back_inserter(result), aStopCount-1, [&]()
            {
                Decimal next = applyTickSize(previous * aFactor, aTickSize);
                if (next <= previous)
                {
                    spdlog::critical("Two consecutive stops are not strictly incremental: {} then {}.",
                            previous,
                            next);
                    throw std::logic_error{"Cannot generate a strictly incremental ladder from arguments."};
                }
                return previous=next;
            });

    return result;
}


void logCritical(const std::string aMessage)
{
    spdlog::critical(aMessage);
}


} // namespace trade
} // namespace ad
