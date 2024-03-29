#include "Ladder.h"

#include "DecimalLog.h"

#include <algorithm>
#include <spdlog/spdlog.h>


namespace ad {
namespace trade {


Ladder makeLadder(Decimal aFirstRate,
                  Decimal aFactor,
                  std::size_t aStopCount,
                  Decimal aEffectivePriceTickSize,
                  Decimal aInternalPriceTickSize,
                  Decimal aPriceOffset)
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

    if (aInternalPriceTickSize > aEffectivePriceTickSize)
    {
        spdlog::critical("Invalid relative tick size, internal's '{}' cannot be bigger than exchange's '{}'.",
                         aInternalPriceTickSize, aEffectivePriceTickSize);
        throw std::domain_error{"Invalid relative tick sizes."};
    }

    Decimal previousInternal = applyTickSizeFloor(aFirstRate, aInternalPriceTickSize);
    Decimal previousExchange = applyTickSizeFloor(aFirstRate + aPriceOffset, aEffectivePriceTickSize);
    Ladder result{previousExchange};

    // Insert the sequence of `nextExchange` values in the `result` Ladder.
    std::generate_n(std::back_inserter(result), aStopCount-1, [&]()
            {
                Decimal nextInternal = applyTickSizeFloor(previousInternal * aFactor, aInternalPriceTickSize);
                Decimal nextExchange = applyTickSizeFloor(nextInternal + aPriceOffset, aEffectivePriceTickSize);
                if (nextExchange <= previousExchange)
                {
                    spdlog::critical("Two consecutive stops are not strictly incremental: {} then {}.",
                            previousExchange,
                            nextExchange);
                    throw std::logic_error{"Cannot generate a strictly incremental ladder from arguments."};
                }
                previousInternal = nextInternal;
                return previousExchange = nextExchange;
            });

    return result;
}


void logCritical(const std::string aMessage)
{
    spdlog::critical(aMessage);
}


} // namespace trade
} // namespace ad
