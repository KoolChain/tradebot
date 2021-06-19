#include <tradebot/Database.h>

#include <trademath/Function.h>
#include <trademath/Ladder.h>
#include <trademath/Spawn.h>

#include <spdlog/spdlog.h>

#include <iostream>

#include <cstdlib>


using namespace ad;


int main(int argc, char * argv[])
{
    if (argc != 9)
    {
        std::cerr << "Usage: " << argv[0] << " database-path base quote amount first-stop factor stop-count tickSize\n";
        return EXIT_FAILURE;
    }

    const std::string databasePath{argv[1]};
    tradebot::Pair pair{argv[2], argv[3]};
    Decimal amount{argv[4]};
    Decimal firstStop{argv[5]};
    Decimal factor{argv[6]};
    int stopsCount = std::stoi(argv[7]);
    Decimal tickSize{argv[8]};

    tradebot::Database database{databasePath};

    Decimal sumBefore = database.sumAllFragments();

    trade::Ladder ladder = trade::makeLadder(firstStop, factor, stopsCount, tickSize);

    spdlog::info("Making a ladder with {} stops on interval [{}, {}].",
            stopsCount,
            ladder.front(),
            ladder.back());

    trade::Function initialDistribution{
        [](Decimal aValue) -> Decimal
        {
            return log(aValue);
        }
    };

    Decimal integral = initialDistribution.integrate(ladder.front(), ladder.back());
    Decimal ratio = fromFP(amount/integral); // lets round it
    spdlog::info("Integration produces {}, target is {}, so the factor is {}.",
            integral,
            amount,
            ratio);

    auto [spawns, spawnedAmount] =
        trade::spawnIntegration(trade::Base{ratio}, ladder.begin(), ladder.end(), initialDistribution);

    if (! isEqual(spawnedAmount, amount))
    {
        spdlog::critical("Spawned amount {} is different from requested amount {}.",
                spawnedAmount,
                amount);
        return EXIT_FAILURE;
    }

    for (const auto & spawn : spawns)
    {
        database.insert(tradebot::Fragment{
            pair.base,
            pair.quote,
            spawn.base,
            spawn.rate,
            tradebot::Side::Sell,
        });
    }

    spdlog::info("Successfully spawned {} fragments for {}.",
        spawns.size(),
        database.sumAllFragments() - sumBefore
    );

    return EXIT_SUCCESS;
}
