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


} // namespace trade
} // namespace ad
