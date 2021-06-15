#pragma once

#include "Database.h"
#include "Fragment.h"

#include <trademath/Spawn.h>


namespace ad {
namespace tradebot {


class SpawnerBase
{
public:
    /// \brief A list of spawn for fulfilling a fragment, and the taken home
    /// The taken home is expressed in **quote** whe fulfilling a `Sell` fragment,
    /// and in **base** when fulfiling a `Buy` fragment.
    using Result = std::pair<std::vector<trade::Spawn>, Decimal /*takenHome*/>;

    SpawnerBase() = default;
    virtual ~SpawnerBase() = default;

    // Suppress copying
    SpawnerBase(const SpawnerBase &) = delete;
    SpawnerBase & operator=(const SpawnerBase &) = delete;

    virtual Result
    computeResultingFragments(const Fragment & aFilledFragment,
                              const FulfilledOrder & aOrder,
                              Database & aDatabase) = 0;
};


/// \brief Does not spawn any counter fragments.
class NullSpawner : public SpawnerBase
{
public:
    Result
    computeResultingFragments(const Fragment & aFilledFragment,
                              const FulfilledOrder & aOrder,
                              Database & aDatabase) override
    {
        switch (aFilledFragment.side)
        {
            case Side::Buy:
                return {{}, aFilledFragment.amount};
            case Side::Sell:
                return {{}, aFilledFragment.amount * aOrder.executionRate};
        }
    }
};


} // namespace tradebot
} // namespace ad