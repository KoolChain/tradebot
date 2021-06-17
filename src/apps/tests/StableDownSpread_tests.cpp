#include "catch.hpp"

#include "Utilities.h"

#include <tradebot/spawners/StableDownSpread.h>

#include <trademath/Spreaders.h>


using namespace ad;
using namespace ad::tradebot;


SCENARIO("StableDownSpread from fixed proportions.", "[sdspread]")
{
    const Pair pair{"BTC", "USDT"};
    const std::string tradername = "sdspreadtest";

    GIVEN("A StableDownSpread with proportion spreader")
    {
        trade::Ladder ladder{
            1,
            2,
            3,
            4,
            5,
            6,
            7,
            8,
            9,
        };

        std::vector<Decimal> proportions{
            Decimal{"0.4"},
            Decimal{"0.6"},
        };

        Decimal initialSellFactor{"0.4"};
        Decimal subsequentSellFactor{"0.2"};
        Decimal subsequentBuyFactor{"0.3"};

        spawner::StableDownSpread<trade::ProportionSpreader> spawner{
            trade::ProportionSpreader{
                ladder,
                proportions
            },
            initialSellFactor,
            subsequentSellFactor,
            subsequentBuyFactor,
        };

        WHEN("An initial sell fragment is present in database.")
        {
            tradebot::Database db{":memory:"};

            Decimal amount{"100"};
            Decimal rate_0{"5"};

            auto [order, fragments] =
                makeOrderWithFragments(db, tradername, pair, {amount}, rate_0, Side::Sell);

            auto fragment = fragments.at(0);

            WHEN("It sells at target rate.")
            {
                Decimal executionRate = rate_0;
                FulfilledOrder fulfilled = mockupFulfill(order, executionRate);
                auto [buySpawns, takenHomeQuote] =
                    spawner.computeResultingFragments(fragment, fulfilled, db);

                THEN("Spawning occurs as expected.")
                {

                    REQUIRE(buySpawns.size() == proportions.size());
                    REQUIRE(takenHomeQuote == amount * executionRate * initialSellFactor);

                    Decimal reinvestedQuote = amount * executionRate - takenHomeQuote;

                    for (std::size_t id = 0; id != proportions.size(); ++id)
                    {
                        INFO("Spawn index is " << id);
                        REQUIRE(buySpawns.at(id).rate == (4 - id));
                        REQUIRE(isEqual(buySpawns.at(id).targetQuote(),
                                        reinvestedQuote * proportions.at(id)));
                    }
                }
            }

            WHEN("It sells at superior rate.")
            {
                Decimal executionRate = rate_0 + Decimal{"1.65"};
                FulfilledOrder fulfilled = mockupFulfill(order, executionRate);
                auto [buySpawns, takenHomeQuote] =
                    spawner.computeResultingFragments(fragment, fulfilled, db);

                THEN("Spawning occurs as expected.")
                {

                    REQUIRE(buySpawns.size() == proportions.size());
                    REQUIRE(takenHomeQuote == amount * executionRate * initialSellFactor);

                    Decimal reinvestedQuote = amount * executionRate - takenHomeQuote;

                    for (std::size_t id = 0; id != proportions.size(); ++id)
                    {
                        INFO("Spawn index is " << id);
                        REQUIRE(buySpawns.at(id).rate == (4 - id));
                        REQUIRE(isEqual(buySpawns.at(id).targetQuote(),
                                        reinvestedQuote * proportions.at(id)));
                    }
                }
            }

            WHEN("A subsequent buy fragment and order are present in database.")
            {
                // fulfill the parent order
                Decimal parentExecutionRate{rate_0 + Decimal{"0.05"}};
                db.update(mockupFulfill(order, parentExecutionRate));

                Decimal amount_1{80};
                Decimal rate_1{4};

                auto [orderBuy_1, fragments] =
                    makeOrderWithFragments(db, tradername, pair, {amount_1}, rate_1, Side::Buy, order.id);
                auto fragmentBuy_1 = fragments.at(0);

                WHEN("It is bought at target rate.")
                {
                    Decimal executionRate = rate_1;
                    FulfilledOrder fulfilled = mockupFulfill(orderBuy_1, executionRate);
                    auto [sellSpawns, takenHomeBase] =
                        spawner.computeResultingFragments(fragmentBuy_1, fulfilled, db);

                    THEN("Spawning occurs as expected.")
                    {

                        REQUIRE(sellSpawns.size() == 1);
                        Decimal excessQuote = amount_1 * (rate_0 - rate_1);
                        Decimal excessBase = excessQuote / rate_0;
                        REQUIRE(takenHomeBase == excessBase * subsequentBuyFactor);

                        Decimal reinvestedBase = amount_1 - takenHomeBase;

                        trade::Spawn spawn = sellSpawns.at(0);
                        REQUIRE(spawn.rate == order.fragmentsRate);
                        REQUIRE(isEqual(spawn.base, reinvestedBase));
                    }

                }

                WHEN("It is bought at an inferior rate.")
                {
                    // This one should have no impact on the taken home: we are not buying
                    // at a fixed amount of quote, but at a fixed amount of base.
                    // So it will not generate more base if the rate is advantageous, but some
                    // extra "taken home quote" for the spawning order (even thought they are not recorded).
                    Decimal executionRate = rate_1 - Decimal{"0.487"};
                    FulfilledOrder fulfilled = mockupFulfill(orderBuy_1, executionRate);
                    auto [sellSpawns, takenHomeBase] =
                        spawner.computeResultingFragments(fragmentBuy_1, fulfilled, db);

                    THEN("Spawning occurs as expected.")
                    {

                        REQUIRE(sellSpawns.size() == 1);
                        Decimal excessQuote = amount_1 * (rate_0 - rate_1);
                        Decimal excessBase = excessQuote / rate_0;
                        REQUIRE(takenHomeBase == excessBase * subsequentBuyFactor);

                        Decimal reinvestedBase = amount_1 - takenHomeBase;

                        trade::Spawn spawn = sellSpawns.at(0);
                        REQUIRE(spawn.rate == order.fragmentsRate);
                        REQUIRE(isEqual(spawn.base, reinvestedBase));
                    }

                }

                //
                // Subsequent sell
                //
                WHEN("A subsequent sell fragment and order are present in database.")
                {
                    // fulfill the parent order
                    Decimal parentExecutionRate{rate_1 - Decimal{"0.8"}};
                    db.update(mockupFulfill(orderBuy_1, parentExecutionRate));

                    Decimal amount_2{100};
                    Decimal rate_2{5};

                    auto [orderSell_2, fragments] =
                        makeOrderWithFragments(db, tradername, pair, {amount_2}, rate_2, Side::Sell, orderBuy_1.id);
                    auto fragmentSell_2 = fragments.at(0);

                    WHEN("It is sold at target rate.")
                    {
                        Decimal executionRate = rate_2;
                        FulfilledOrder fulfilled = mockupFulfill(orderSell_2, executionRate);
                        auto [buySpawns, takenHomeQuote] =
                            spawner.computeResultingFragments(fragmentSell_2, fulfilled, db);

                        THEN("Spawning occurs as expected.")
                        {

                            REQUIRE(buySpawns.size() == 1);
                            Decimal excessQuote = amount_2 * (executionRate - rate_1);
                            REQUIRE(takenHomeQuote == excessQuote * subsequentSellFactor);

                            Decimal reinvestedQuote = (amount * executionRate) - takenHomeQuote;

                            trade::Spawn spawn = buySpawns.at(0);
                            REQUIRE(spawn.rate == orderBuy_1.fragmentsRate);
                            REQUIRE(isEqual(spawn.targetQuote(), reinvestedQuote));
                        }

                    }

                    WHEN("It is sold at a superior rate.")
                    {
                        Decimal executionRate = rate_2 + Decimal{"8.63"};
                        FulfilledOrder fulfilled = mockupFulfill(orderSell_2, executionRate);
                        auto [buySpawns, takenHomeQuote] =
                            spawner.computeResultingFragments(fragmentSell_2, fulfilled, db);

                        THEN("Spawning occurs as expected.")
                        {

                            REQUIRE(buySpawns.size() == 1);
                            Decimal excessQuote = amount_2 * (executionRate - rate_1);
                            REQUIRE(takenHomeQuote == excessQuote * subsequentSellFactor);

                            Decimal reinvestedQuote = (amount * executionRate) - takenHomeQuote;

                            trade::Spawn spawn = buySpawns.at(0);
                            REQUIRE(spawn.rate == orderBuy_1.fragmentsRate);
                            REQUIRE(isEqual(spawn.targetQuote(), reinvestedQuote));
                        }

                    }
                }
            }
        }

    }
}
