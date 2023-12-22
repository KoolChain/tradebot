#pragma once


#include "../Spawner.h"

#include <tradebot/Logging.h>

#include <trademath/Spawn.h>


namespace ad {
namespace tradebot {
namespace spawner {

// Design notes:
// After the initial `Sell`, spawn buy fragments for a proportion of the **quote** amount generated by the sale
// (the rest is the taken home).
// The **real** quote amount is considered. This is an important distiction if the sale took place at
// at a rate above the target.
// The buy fragments are distributed down following a discrete sequence of factors.
// The sum of factors has to be == 1 (the taken home being already removed from the distributed amount).
//
// After a `Buy`, spawn sell fragments at the parent's buy rate (thus cycling).
// TODO define if some is taken home
// It should be made sure that it does not lead to a lowering cycle: the base amount to sell
// should provide a quote amount at least equal to the quote amount spent during this `Buy`, plus
// some if some should be taken home after the `Sell`.


#define TMP_PARAM_LIST class T_spreader
#define TMP_ARGS T_spreader


template <TMP_PARAM_LIST>
class StableDownSpread : public SpawnerBase
{
private:
    void filterSpawnedAmount(Decimal & aSpawned, Decimal & aTakenHome);

public:
    StableDownSpread(T_spreader aSpreader,
                     Decimal aTakeHomeFactorInitialSell,
                     Decimal aTakeHomeFactorSubsequentSell,
                     Decimal aTakeHomeFactorSubsequentBuy);

    Decimal tickSize() const;

    Result
    computeResultingFragments(const Fragment & aFilledFragment,
                              const FulfilledOrder & aOrder,
                              Database & aDatabase) override;

    Result onFirstSell(const Fragment & aFilledFragment,
                       const FulfilledOrder & aOrder);

    Result onSubsequentBuy(const Fragment & aFilledFragment,
                           const FulfilledOrder & aOrder,
                           Database & aDatabase);

    Result onSubsequentSell(const Fragment & aFilledFragment,
                            const FulfilledOrder & aOrder,
                            Database & aDatabase);

    T_spreader downSpreader;

    // All those factors should be strictly in [0, 1]
    Decimal takeHomeFactorInitialSell;
    Decimal takeHomeFactorSubsequentSell;
    Decimal takeHomeFactorSubsequentBuy;
};


template <TMP_PARAM_LIST>
StableDownSpread<TMP_ARGS>::StableDownSpread(
        T_spreader aSpreader,
        Decimal aTakeHomeFactorInitialSell,
        Decimal aTakeHomeFactorSubsequentSell,
        Decimal aTakeHomeFactorSubsequentBuy) :
    downSpreader{std::move(aSpreader)},
    takeHomeFactorInitialSell{aTakeHomeFactorInitialSell},
    takeHomeFactorSubsequentSell{aTakeHomeFactorSubsequentSell},
    takeHomeFactorSubsequentBuy{aTakeHomeFactorSubsequentBuy}
{}


template <TMP_PARAM_LIST>
Decimal StableDownSpread<TMP_ARGS>::tickSize() const
{
    return downSpreader.amountTickSize;
}


template <TMP_PARAM_LIST>
SpawnerBase::Result
StableDownSpread<TMP_ARGS>::computeResultingFragments(const Fragment & aFilledFragment,
                                                      const FulfilledOrder & aOrder,
                                                      Database & aDatabase)
{
    switch (aFilledFragment.side)
    {
        case Side::Sell:
        {
            if (aFilledFragment.isInitial())
            {
                return onFirstSell(aFilledFragment, aOrder);
            }
            else
            {
                return onSubsequentSell(aFilledFragment, aOrder, aDatabase);
            }
        }
        case Side::Buy:
        {
            if (aFilledFragment.isInitial())
            {
                spdlog::critical("StableDownSpread cannot handle initial Buy fragments: '{}'",
                    boost::lexical_cast<std::string>(aFilledFragment));
                throw std::logic_error{"StableDownSpread encountered an initial Buy fragment."};
            }
            return onSubsequentBuy(aFilledFragment, aOrder, aDatabase);
        }
        default:
            throw std::domain_error{"Invalid Side enumerator."};
    }
}


template <TMP_PARAM_LIST>
SpawnerBase::Result
StableDownSpread<TMP_ARGS>::onFirstSell(const Fragment & aFilledFragment,
                                        const FulfilledOrder & aOrder)
{
    Decimal actualQuoteAmount{aFilledFragment.baseAmount * aOrder.executionRate};

    // Note: The spreader takes care of applying tick size filter itself, as long as it was
    // constructed with one.
    auto [spawns, totalSpawnedQuote] =
        downSpreader.spreadDown(trade::Quote{actualQuoteAmount * (1-takeHomeFactorInitialSell)},
                                aFilledFragment.targetRate);

    Decimal takenHome = actualQuoteAmount - static_cast<Decimal>(totalSpawnedQuote);
    return {spawns, takenHome};
}


template <TMP_PARAM_LIST>
SpawnerBase::Result
StableDownSpread<TMP_ARGS>::onSubsequentBuy(const Fragment & aFilledFragment,
                                            const FulfilledOrder & aOrder,
                                            Database & aDatabase)
{
    Order parentOrder = aDatabase.getOrder(aFilledFragment.spawningOrder);
    Decimal parentSellRate = parentOrder.fragmentsRate;

    // Sanity check
    {
        if (parentSellRate <= aFilledFragment.targetRate)
        {
            spdlog::critical("The buy fragment '{}' has an higher price ({}) than its parent order '{}' ().",
                             aFilledFragment.getIdentity(),
                             aFilledFragment.targetRate,
                             aOrder.getIdentity(),
                             parentSellRate);
            throw std::logic_error{"The parent order of a buy fragment must have a strictly superior price."};
        }
    }

    // The amount of base obtained through the current buy fragment
    Decimal actualBaseAmount = aFilledFragment.baseAmount;
    // The quote amount required to buy this fragment's base at the current target rate.
    // i.e. How much quote the next sell should provide to be stable.
    Decimal breakEvenQuote = actualBaseAmount * aFilledFragment.targetRate;

    // The minimal amount of base to sell at parents rate
    // in order to get enough quote to buy breakEvenQuote again (at current rate).
    Decimal breakEvenBase = breakEvenQuote / parentSellRate;

    Decimal excessBase = actualBaseAmount - breakEvenBase;
    Decimal takenHomeBase = excessBase * takeHomeFactorSubsequentBuy;

    //
    // Tick size filtering
    //

    // estimate of spawned, before filtering
    trade::Spawn singleSpawn{parentSellRate, trade::Base{actualBaseAmount - takenHomeBase}};
    // filter to find the exact amount that will be spawned
    singleSpawn.base = trade::applyTickSize(singleSpawn.base, tickSize());
    // compute the exact taken home, from exact spawned
    takenHomeBase = actualBaseAmount - singleSpawn.base;

    return {{singleSpawn}, takenHomeBase};
}


template <TMP_PARAM_LIST>
SpawnerBase::Result
StableDownSpread<TMP_ARGS>::onSubsequentSell(const Fragment & aFilledFragment,
                                             const FulfilledOrder & aOrder,
                                             Database & aDatabase)
{
    Order parentOrder = aDatabase.getOrder(aFilledFragment.spawningOrder);
    Decimal parentBuyRate = parentOrder.fragmentsRate;

    // Sanity check
    {
        if (parentBuyRate >= aFilledFragment.targetRate)
        {
            spdlog::critical("The sell fragment '{}' has a lower price ({}) than its parent order '{}' ({}).",
                             aFilledFragment.getIdentity(),
                             aFilledFragment.targetRate,
                             aOrder.getIdentity(),
                             parentBuyRate);
            throw std::logic_error{"The parent order of a sell fragment must have a strictly inferior price."};
        }
    }

    // The amount of quote obtained throught the current sell fragment.
    Decimal actualQuoteAmount = aFilledFragment.baseAmount * aOrder.executionRate;
    // How much base the next buy should provide to be stable.
    Decimal breakEvenBase = aFilledFragment.baseAmount;

    // The minimal amount of quote to buy base with at parents rate
    // in order to get enough base to sell breakEvenBase again (at current rate).
    Decimal breakEvenQuote = breakEvenBase * parentBuyRate;

    Decimal excessQuote = actualQuoteAmount - breakEvenQuote;
    Decimal takenHomeQuote = excessQuote * takeHomeFactorSubsequentSell;

    //
    // Tick size filtering
    //

    // The computation occured in quote, but the order will be placed in base
    // so the tick filter has to be applied to the base amount.
    // (Then, the filtered base amount is converted back to quote, to find the exact taken home)
    trade::Spawn singleSpawn{parentBuyRate, trade::Quote{actualQuoteAmount - takenHomeQuote}};
    singleSpawn.base = trade::applyTickSize(singleSpawn.base, tickSize());
    takenHomeQuote = actualQuoteAmount - singleSpawn.getAmount<trade::Quote>();

    return {{singleSpawn}, takenHomeQuote};
}


#undef TMP_ARGS
#undef TMP_PARAM_LIST


} // namespace spawner
} // namespace tradebot
} // namespace ad
