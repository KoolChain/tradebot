#include <tradebot/Database.h>
#include <tradebot/Exchange.h>

#include <trademath/Function.h>
#include <trademath/Ladder.h>
#include <trademath/Spawn.h>

#include <spdlog/spdlog.h>

#include <fstream>
#include <iostream>

#include <cstdlib>


using namespace ad;


int main(int argc, char * argv[])
{
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " secretsfile database-path config-path\n";
        return EXIT_FAILURE;
    }

    const std::string secretsfile{argv[1]};
    const std::string databasePath{argv[2]};

    Json config = Json::parse(std::ifstream{argv[3]});
    tradebot::Pair pair{config.at("base"), config.at("quote")};
    Decimal amount{config.at("amount").get<std::string>()};
    Decimal firstStop{config.at("ladder").at("firstStop").get<std::string>()};
    Decimal factor{config.at("ladder").at("factor").get<std::string>()};
    int stopCount = std::stoi(config.at("ladder").at("stopCount").get<std::string>());
    Decimal tickSize{config.at("ladder").at("tickSize").get<std::string>()};

    tradebot::Exchange exchange{binance::Api{std::ifstream{secretsfile}}};
    tradebot::SymbolFilters filters = exchange.queryFilters(pair);

    trade::Ladder ladder = trade::makeLadder(firstStop, factor, stopCount, tickSize);

    // Sanity check
    {
        if (ladder.front() < filters.price.minimum || ladder.back() > filters.price.maximum)
        {
            spdlog::critical("Ladder interval [{}, {}] is not contained in "
                             "exchange's price interval [{}, {}].",
                             ladder.front(), ladder.back(),
                             filters.price.minimum, filters.price.maximum);
            throw std::invalid_argument{"Configured ladder is not contained withing the exchange's price interval."};
        }
        if (trade::applyTickSize(tickSize, filters.price.tickSize) != tickSize)
        {
            spdlog::critical("Configured price tick-size {} is too permissive compared "
                             "to exchange's tick size {}.",
                             tickSize,
                             filters.price.tickSize);
            throw std::invalid_argument{"Configured price tick-size is not compatible with the filters on the exchange."};
        }
    }

    spdlog::info("Making a ladder with {} stops on interval [{}, {}], tick size {}.",
            stopCount,
            ladder.front(),
            ladder.back(),
            tickSize);

    trade::Function initialDistribution{
        [](Decimal aValue) -> Decimal
        {
            return log(aValue);
        }
    };

    Decimal integral = initialDistribution.integrate(ladder.front(), ladder.back());
    Decimal ratio = fromFP(amount/integral); // lets round it
    spdlog::debug("Integration produces {}, target is {}, so the factor is {}.",
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

    tradebot::Database database{databasePath};
    Decimal sumBefore = database.sumAllFragments();
    Decimal remainder{0};
    for (const auto & spawn : spawns)
    {
        // Carry-on the remainder from previous amount insertion
        Decimal baseAmount;
        std::tie(baseAmount, remainder) =
            trade::computeTickFilter(spawn.base + remainder, filters.amount.tickSize);

        if (! tradebot::testAmount(filters, baseAmount, spawn.rate))
        {
            spdlog::warn("Spawned fragment for {} {} at rate {} does not pass amount filters.",
                         baseAmount,
                         pair.base,
                         spawn.rate);
        }

        database.insert(tradebot::Fragment{
            pair.base,
            pair.quote,
            baseAmount,
            spawn.rate,
            tradebot::Side::Sell,
        });
    }

    spdlog::info("Successfully spawned {} fragments, changing the total sum of fragments in DB for {}.",
        spawns.size(),
        database.sumAllFragments() - sumBefore
    );

    return EXIT_SUCCESS;
}
