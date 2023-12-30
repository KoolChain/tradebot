#include "NaiveDownSpread.h"

#include "Helpers.h"

#include <trademath/Spawn.h>


namespace ad {
namespace tradebot {
namespace spawner {


NaiveDownSpread::NaiveDownSpread(trade::Ladder aLadder, trade::ProportionsMap aProportions) :
    downSpreader{
        std::move(aLadder),
        std::move(aProportions)
    }
{}


SpawnerBase::Result NaiveDownSpread::computeResultingFragments(const Fragment & aFilledFragment,
                                                               const FulfilledOrder & aOrder,
                                                               Database & aDatabase)
{
    switch (aFilledFragment.side)
    {
        case Side::Sell:
        {
            auto [result, accumulatedBase] =
                downSpreader.spreadDown(trade::Base{aFilledFragment.baseAmount},
                                        aFilledFragment.targetRate);

            return {result, aFilledFragment.baseAmount * aOrder.executionRate - sumSpawnQuote(result)};
        }
        case Side::Buy:
        {
            Order parentOrder = aDatabase.getOrder(aFilledFragment.spawningOrder);
            // taken home is 0
            return {
                {trade::Spawn{parentOrder.fragmentsRate, trade::Base{aFilledFragment.baseAmount}}},
                Decimal{0}
            };
        }
        default:
            throw std::domain_error{"Invalid Side enumerator."};
    }
}


} // namespace spawner
} // namespace tradebot
} // namespace ad
