#include "NaiveDownSpread.h"

#include "Helpers.h"

#include <trademath/Spawn.h>


namespace ad {
namespace tradebot {
namespace spawner {


NaiveDownSpread::NaiveDownSpread(trade::Ladder aLadder, std::vector<Decimal> aProportions) :
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
                downSpreader.spreadDown(trade::Base{aFilledFragment.amount},
                                        aFilledFragment.targetRate);

            return {result, aFilledFragment.amount * aOrder.executionRate - sumSpawnQuote(result)};
        }
        case Side::Buy:
        {
            Order parentOrder = aDatabase.getOrder(aFilledFragment.spawningOrder);
            // taken home is 0
            return {
                {trade::Spawn{parentOrder.fragmentsRate, trade::Base{aFilledFragment.amount}}},
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
