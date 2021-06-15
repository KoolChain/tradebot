#include "catch.hpp"

#include "Utilities.h"

#include <tradebot/Trader.h>
#include <tradebot/spawners/NaiveDownSpread.h>

#include <trademath/Function.h>
#include <trademath/Ladder.h>
#include <trademath/Spawn.h>

#include <cmath>


using namespace ad;
using namespace ad::trade;
using namespace ad::tradebot;


SCENARIO("Ladder generation", "[spawn]")
{
    GIVEN("A factor of 2.")
    {
        Decimal factor{"2"};
        WHEN("A ladder is instantiated.")
        {
            const std::size_t count = 6;
            Ladder ladder = trade::makeLadder(Decimal{"1"}, factor, count);

            THEN("It has correct size and stops.")
            {
                REQUIRE(ladder.size() == count);

                const auto expected = {1, 2, 4, 8, 16, 32};
                REQUIRE(std::equal(ladder.begin(), ladder.end(),
                                   expected.begin(), expected.end()));
            }
        }
    }

    GIVEN("A factor of 1.1")
    {
        Decimal factor{"1.06"};
        WHEN("A ladder is instantiated.")
        {
            const std::size_t count = 45;
            Ladder ladder = trade::makeLadder(Decimal{"0.2"}, factor, count, Decimal{"0.0001"});

            THEN("It has correct size and stops.")
            {
                REQUIRE(ladder.size() == count);

                // Generated in a spreadsheet
                const auto expected = {
                    Decimal{"0.2000"},
                    Decimal{"0.2120"},
                    Decimal{"0.2247"},
                    Decimal{"0.2381"},
                    Decimal{"0.2523"},
                    Decimal{"0.2674"},
                    Decimal{"0.2834"},
                    Decimal{"0.3004"},
                    Decimal{"0.3184"},
                    Decimal{"0.3375"},
                    Decimal{"0.3577"},
                    Decimal{"0.3791"},
                    Decimal{"0.4018"},
                    Decimal{"0.4259"},
                    Decimal{"0.4514"},
                    Decimal{"0.4784"},
                    Decimal{"0.5071"},
                    Decimal{"0.5375"},
                    Decimal{"0.5697"},
                    Decimal{"0.6038"},
                    Decimal{"0.6400"},
                    Decimal{"0.6784"},
                    Decimal{"0.7191"},
                    Decimal{"0.7622"},
                    Decimal{"0.8079"},
                    Decimal{"0.8563"},
                    Decimal{"0.9076"},
                    Decimal{"0.9620"},
                    Decimal{"1.0197"},
                    Decimal{"1.0808"},
                    Decimal{"1.1456"},
                    Decimal{"1.2143"},
                    Decimal{"1.2871"},
                    Decimal{"1.3643"},
                    Decimal{"1.4461"},
                    Decimal{"1.5328"},
                    Decimal{"1.6247"},
                    Decimal{"1.7221"},
                    Decimal{"1.8254"},
                    Decimal{"1.9349"},
                    Decimal{"2.0509"},
                    Decimal{"2.1739"},
                    Decimal{"2.3043"},
                    Decimal{"2.4425"},
                    Decimal{"2.5890"},
                };

                std::size_t i=0;
                for (const auto expect : expected)
                {
                    REQUIRE(expect == ladder.at(i++));
                }
            }
        }
    }
}


SCENARIO("Spawning from function, hardcoded 50K.", "[spawn]")
{
    GIVEN("A ladder and a distribution function.")
    {
        Ladder ladder = trade::makeLadder(Decimal{"0.2"}, Decimal{"1.03"}, 55, Decimal{"0.0001"});
        // Sanity check, value computed offline
        REQUIRE(ladder.back() == Decimal{"0.9798"});

        Function initialDistribution{
            [](Decimal aValue)
            {
                return Decimal("31465.715") * log(aValue);
            }
        };

        THEN("We can integrate the distribution over the whole interval.")
        {
            Decimal integral = initialDistribution.integrate(ladder.front(), ladder.back());

            // The factor was tailored to distribute just below 50000
            REQUIRE(integral > Decimal{"49999.99"});
            REQUIRE(integral < Decimal{"50000"});

            WHEN("A distribution over the ladder can be computed.")
            {
                auto [spawnee, totalSpawned] = spawnIntegration(
                        Base{1},
                        ladder.begin(), ladder.end(),
                        initialDistribution);

                THEN("There is one spawn per stop, excluding first.")
                {
                    // Spawning on intervals, so numbers of stops - 1
                    REQUIRE(spawnee.size() == ladder.size() - 1);

                    REQUIRE(std::equal(spawnee.begin(), spawnee.end(),
                                       ladder.begin()+1, ladder.end(),
                                       [](auto spawn, auto stop)
                                       {
                                        return spawn.rate == stop;
                                       }));
                }

                THEN("The distributed value over the ladder is equal to the integral over the interval.")
                {
                    Decimal sum = sumSpawnBase(spawnee);
                    REQUIRE(sum == integral);
                    REQUIRE(sum == totalSpawned);
                    REQUIRE(sum != sumSpawnQuote(spawnee));
                }
            }
        }
    }
}


SCENARIO("Spawning from function.", "[spawn]")
{
    GIVEN("A ladder and a distribution function.")
    {
        Ladder ladder = trade::makeLadder(Decimal{"1"}, Decimal{"2"}, 5);
        // Sanity check, value computed offline
        REQUIRE(ladder.back() == Decimal{"16"});

        Function distribution{
            [](Decimal aValue)
            {
                // Primitive of f(x) -> x
                return pow(aValue, 2) / Decimal{"2"};
            }
        };

        THEN("We can integrate the distribution over the whole interval.")
        {
            Decimal integral = distribution.integrate(ladder.front(), ladder.back());

            REQUIRE(integral == Decimal{"127.5"});

            WHEN("Amounts are spawned upward from base.")
            {
                Base amount{100};
                auto [spawned, totalSpawned] =
                    spawnIntegration(amount,
                                     ladder.begin(), ladder.end(),
                                     distribution);

                THEN("It results in expected amounts.")
                {
                    REQUIRE(spawned.size() == 4);
                    CHECK(totalSpawned == Base{(Decimal)amount * integral});

                    CHECK(spawned.at(0) == Spawn{ladder.at(1), Base{150}});
                    CHECK(spawned.at(1) == Spawn{ladder.at(2), Base{600}});
                    CHECK(spawned.at(2) == Spawn{ladder.at(3), Base{2400}});
                    CHECK(spawned.at(3) == Spawn{ladder.at(4), Base{9600}});
                }
            }

            WHEN("Amounts are spawned downward from base.")
            {
                Base amount{100};
                auto [spawned, totalSpawned] =
                    spawnIntegration(amount,
                                     ladder.rbegin(), ladder.rend(),
                                     distribution);

                THEN("It results in expected amounts.")
                {
                    REQUIRE(spawned.size() == 4);
                    CHECK(totalSpawned == Base{(Decimal)amount * integral});

                    CHECK(spawned.at(0) == Spawn{ladder.at(3), Base{9600}});
                    CHECK(spawned.at(1) == Spawn{ladder.at(2), Base{2400}});
                    CHECK(spawned.at(2) == Spawn{ladder.at(1), Base{600}});
                    CHECK(spawned.at(3) == Spawn{ladder.at(0), Base{150}});
                }
            }

            WHEN("Amounts are spawned upward from quote.")
            {
                Quote amount{10};
                auto [spawned, totalSpawned] =
                    spawnIntegration(amount,
                                     ladder.begin(), ladder.end(),
                                     distribution);

                THEN("It results in expected amounts.")
                {
                    REQUIRE(spawned.size() == 4);
                    CHECK(totalSpawned == Quote{(Decimal)amount * integral});

                    std::vector<Base> expectedBases = {
                        Base{Decimal{15} / 2},
                        Base{Decimal{60} / 4},
                        Base{Decimal{240} / 8},
                        Base{Decimal{960} / 16},
                    };

                    CHECK(spawned.at(0) == Spawn{ladder.at(1), expectedBases.at(0)});
                    CHECK(spawned.at(1) == Spawn{ladder.at(2), expectedBases.at(1)});
                    CHECK(spawned.at(2) == Spawn{ladder.at(3), expectedBases.at(2)});
                    CHECK(spawned.at(3) == Spawn{ladder.at(4), expectedBases.at(3)});
                }
            }

            WHEN("Amounts are spawned downward from quote.")
            {
                Quote amount{10};
                auto [spawned, totalSpawned] =
                    spawnIntegration(amount,
                                     ladder.rbegin(), ladder.rend(),
                                     distribution);

                THEN("It results in expected amounts.")
                {
                    REQUIRE(spawned.size() == 4);
                    CHECK(totalSpawned == Quote{(Decimal)amount * integral});

                    std::vector<Base> expectedBases = {
                        Base{Decimal{960} / 8},
                        Base{Decimal{240} / 4},
                        Base{Decimal{60} / 2},
                        Base{Decimal{15} / 1},
                    };

                    CHECK(spawned.at(0) == Spawn{ladder.at(3), expectedBases.at(0)});
                    CHECK(spawned.at(1) == Spawn{ladder.at(2), expectedBases.at(1)});
                    CHECK(spawned.at(2) == Spawn{ladder.at(1), expectedBases.at(2)});
                    CHECK(spawned.at(3) == Spawn{ladder.at(0), expectedBases.at(3)});
                }
            }

        }
    }
}


SCENARIO("Spawning from proportions.", "[spawn]")
{
    GIVEN("A ladder and proportions.")
    {
        Ladder ladder = trade::makeLadder(Decimal{"1"}, Decimal{"2"}, 5);

        const std::vector<Decimal> proportions = {
            Decimal{"0.5"},
            Decimal{"0.3"},
            Decimal{"0.2"},
        };

        WHEN("Amounts are spawned upward from base.")
        {
            Base amount{1000};
            auto [spawned, totalSpawned] =
                spawnProportions(amount,
                                 ladder.begin()+1, ladder.end(),
                                 proportions.begin(), proportions.end());

            THEN("It results in expected amounts.")
            {
                REQUIRE(spawned.size() == 3);
                CHECK(totalSpawned == amount);
                CHECK(spawned.at(0) == Spawn{ladder.at(1), Base{500}});
                CHECK(spawned.at(1) == Spawn{ladder.at(2), Base{300}});
                CHECK(spawned.at(2) == Spawn{ladder.at(3), Base{(Decimal)amount * proportions[2]}});
            }
        }

        WHEN("Amounts are spawned downward from base.")
        {
            Base amount{500};
            auto [spawned, totalSpawned] =
                spawnProportions(amount,
                                 ladder.rbegin()+1, ladder.rend(),
                                 proportions.begin(), proportions.end());

            THEN("It results in expected amounts.")
            {
                REQUIRE(spawned.size() == 3);
                CHECK(totalSpawned == amount);
                CHECK(spawned.at(0) == Spawn{ladder.at(3), Base{(Decimal)amount * proportions[0]}});
                CHECK(spawned.at(1) == Spawn{ladder.at(2), Base{(Decimal)amount * proportions[1]}});
                CHECK(spawned.at(2) == Spawn{ladder.at(1), Base{(Decimal)amount * proportions[2]}});
            }
        }

        WHEN("Amounts are spawned upward from quote.")
        {
            Quote amount{Decimal{"0.1"}};
            auto [spawned, totalSpawned] =
                spawnProportions(amount,
                                 ladder.begin()+2, ladder.end(),
                                 proportions.begin(), proportions.end());

            THEN("It results in expected amounts.")
            {
                REQUIRE(spawned.size() == 3);
                CHECK(totalSpawned == amount);
                REQUIRE(ladder.at(2) == 4);
                CHECK(spawned.at(0) == Spawn{ladder.at(2), Base{Decimal{"0.05"} / 4}});
                REQUIRE(ladder.at(3) == 8);
                CHECK(spawned.at(1) == Spawn{ladder.at(3), Base{Decimal{"0.03"} / 8}});
                REQUIRE(ladder.at(4) == 16);
                CHECK(spawned.at(2) == Spawn{ladder.at(4), Base{Decimal{"0.02"} / 16}});
            }
        }

        WHEN("Amounts are spawned downward from quote.")
        {
            Quote amount{Decimal{"0.1"}};
            auto [spawned, totalSpawned] =
                spawnProportions(amount,
                                 ladder.rbegin()+2, ladder.rend(),
                                 proportions.begin(), proportions.end());

            THEN("It results in expected amounts.")
            {
                REQUIRE(spawned.size() == 3);
                CHECK(totalSpawned == amount);
                REQUIRE(ladder.at(2) == 4);
                CHECK(spawned.at(0) == Spawn{ladder.at(2), Base{Decimal{"0.05"} / 4}});
                REQUIRE(ladder.at(1) == 2);
                CHECK(spawned.at(1) == Spawn{ladder.at(1), Base{Decimal{"0.03"} / 2}});
                REQUIRE(ladder.at(0) == 1);
                CHECK(spawned.at(2) == Spawn{ladder.at(0), Base{Decimal{"0.02"} / 1}});
            }
        }
    }
}


SCENARIO("Spawning counter-fragments with NaiveDownSpread", "[spawn]")
{
    const Ladder ladder = {
        Decimal{"0.01"},
        Decimal{"0.1"},
        Decimal{"1"},
        Decimal{"10"},
        Decimal{"100"},
        Decimal{"1000"},
        Decimal{"10000"},
        Decimal{"100000"},
    };

    const std::vector<Decimal> proportions = {
        Decimal{"0.05"},
        Decimal{"0.10"},
        Decimal{"0.20"},
        Decimal{"0.25"},
    };

    GIVEN("A trader with a NaiveDownSpread spawner.")
    {
        Pair pair{"BTC", "USDT"};
        Trader trader = makeTrader("spawntest", pair);
        trader.spawner = std::make_unique<spawner::NaiveDownSpread>(ladder, proportions);

        auto & db = trader.database;
        auto & binance = trader.exchange;

        GIVEN("A Sell fragment and its corresponding order.")
        {
            Decimal amount{"1000"};
            Decimal rate{"100"};
            Fragment fragment{
                pair.base,
                pair.quote,
                amount,
                rate,
                Side::Sell,
            };
            db.insert(fragment);

            Order order = db.prepareOrder(
                    trader.name,
                    Side::Sell,
                    rate,
                    pair,
                    Order::FulfillResponse::SmallSpread);
            db.reload(fragment);

            // Sanity check
            REQUIRE(order.amount == amount);
            REQUIRE(fragment.composedOrder == order.id);

            WHEN("The order is completed.")
            {
                mockupFulfillOrder(order, order.fragmentsRate);

                THEN("Resulting 'spawns' can be computed directly on the spawner.")
                {
                    auto [spawns, takenHome] =
                        trader.spawner->computeResultingFragments(fragment, order, db);

                    REQUIRE(spawns.size() == 4);
                    CHECK(spawns.at(0) == Spawn{10, Base{amount * Decimal{"0.05"}}});
                    CHECK(spawns.at(3) == Spawn{Decimal{"0.01"}, Base{amount * Decimal{"0.25"}}});

                    Decimal reinvestedQuote = amount * (0.05*10 + 0.1*1 + 0.2*0.1 + 0.25*0.01);
                    CHECK(isEqual(takenHome, order.executionQuoteAmount() - reinvestedQuote));
                }
            }
        }
    }
}
