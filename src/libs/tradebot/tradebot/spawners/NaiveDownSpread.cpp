#include "NaiveDownSpread.h"

#include "Helpers.h"

#include <trademath/Spawn.h>


namespace ad {
namespace tradebot {
namespace spawner {


NaiveDownSpread::NaiveDownSpread(trade::Ladder aLadder, std::vector<Decimal> aProportions) :
    ladder{std::move(aLadder)},
    proportions{std::move(aProportions)}
{}


SpawnerBase::Result NaiveDownSpread::computeResultingFragments(const Fragment & aFilledFragment,
                                                               const Order & aOrder,
                                                               Database & aDatabase)
{
    switch (aFilledFragment.side)
    {
        case Side::Sell:
        {
            auto reverseStop = getStopFor(ladder.rbegin(), ladder.rend(), aFilledFragment);
            // reverseStop will never be ladder.rend(): getStopFor() throws when stop is not found.
            auto [result, accumulatedBase] =
                trade::spawnProportions(trade::Base{aFilledFragment.amount},
                                        // reverseStop+1 because no fagment should be assigned to the current stop.
                                        reverseStop+1, ladder.rend(),
                                        proportions.begin(), proportions.end());
            return {result, aOrder.executionQuoteAmount()-sumSpawnQuote(result)};
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
    }
}


} // namespace spawner
} // namespace tradebot
} // namespace ad
