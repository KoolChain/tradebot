#pragma once

#include "Decimal.h"
#include "FilterUtilities.h"

#include <vector>


namespace ad {
namespace trade {


using Ladder = std::vector<Decimal>;


/// \param aEffectivePriceTickSize The tick size for the resulting ladder stops,
/// which will be the tick size used for trades sent to the exchange.
/// \param aInternalPriceTickSize The tick size used internally when generating the values.
/// This is the tick size the "previous" value is stored with.
/// \param aPriceOffset Offset that will be added to the generated exchange stop value
/// (before applying aEffectivePriceTickSize).
/// It will not influence the overall ladder growth, just offset all stops by the same amount.
///
/// \important The distinct effective and internal tick size were introduced when binance
/// changed the filter price tick size on a traded pair already in production:
/// it was needed to generate a ladder with the new price tick, while producing values following
/// the previous tick size.
Ladder makeLadder(Decimal aFirstRate,
                  Decimal aFactor,
                  std::size_t aStopCount,
                  Decimal aEffectivePriceTickSize,
                  Decimal aInternalPriceTickSize,
                  Decimal aPriceOffset);

inline Ladder makeLadder(Decimal aFirstRate,
                         Decimal aFactor,
                         std::size_t aStopCount,
                         Decimal aTickSize = gDefaultTickSize)
{ return makeLadder(aFirstRate, aFactor, aStopCount, aTickSize, aTickSize, 0); }

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
