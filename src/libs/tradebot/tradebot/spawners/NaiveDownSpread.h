#pragma once

#include "../Spawner.h"

#include <trademath/Spreaders.h>


namespace ad {
namespace tradebot {
namespace spawner {


/// \brief Always spread down a `Sell` following the same proportions, then for each `Buy` spawn
/// a single 100% `Sell` order at the original `Sell` price.
///
/// The implicit taken home is the remaining quote that was not spread down after a `Sell`.
/// This spawner is naive, because it can easily exhaust a stop by spreading it down continuously
/// (in case where the rate alternates for some period between two neighbor stops).
class NaiveDownSpread : public SpawnerBase
{
public:
    NaiveDownSpread(trade::Ladder aLadder, trade::ProportionsMap aProportions);

    Result
    computeResultingFragments(const Fragment & aFilledFragment,
                              const FulfilledOrder & aOrder,
                              Database & aDatabase) override;


    trade::ProportionSpreader downSpreader;
};


} // namespace spawner
} // namespace tradebot
} // namespace ad
