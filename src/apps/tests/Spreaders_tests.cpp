#include "catch.hpp"

#include <trademath/Spreaders.h>


using namespace ad;
using namespace ad::trade;


SCENARIO("Spreader tick size", "[spreaders]")
{
    GIVEN("A proportion spreader without tick size")
    {
        Ladder ladder{
            1,
            2,
        };

        ProportionSpreader spreader{
            ladder,
            ProportionsMap{
                {
                    Decimal{1000},
                    {
                        Decimal{"0.5"},
                        Decimal{"0.5"},
                    },
                }
            },
        };

        THEN("It spreads with full precision as expected")
        {
            Decimal amount{"1000.5"};
            auto [spawns, accumulation] = spreader.spreadDown(Base{amount}, ladder.at(1));

            REQUIRE(spawns.size() == 1);
            CHECK(spawns.at(0).base == amount);
            CHECK(spawns.at(0).base == accumulation);

            auto [filtered, remainder] = computeTickFilter(accumulation, Decimal{1});
            CHECK(remainder == Decimal{"0.5"});


            WHEN("The spreader is given a tick size of 1.")
            {
                spreader.amountTickSize = Decimal{1};

                THEN("It spreads integers.")
                {
                    auto [spawns, accumulation] = spreader.spreadDown(Base{amount}, ladder.at(1));

                    REQUIRE(spawns.size() == 1);
                    CHECK(spawns.at(0).base == Decimal{1000});
                    CHECK(spawns.at(0).base == accumulation);

                    auto [filtered, remainder] = computeTickFilter(accumulation, Decimal{1});
                    CHECK(remainder == Decimal{0});
                }
            }
        }
    }
}


SCENARIO("Spreader at ladder bound", "[spreaders]")
{
    GIVEN("A proportion spreader and a 4 stops ladder.")
    {
        Ladder ladder{
            1,
            2,
            3,
            4,
        };

        ProportionSpreader spreader{
            ladder,
            ProportionsMap{
                {
                    Decimal{1000},
                    {
                        Decimal{"0.5"},
                        Decimal{"0.5"},
                    },
                }
            },
        };

        WHEN("It spreads down from the first ladder stop.")
        {
            Decimal amount{1000};
            auto [spawns, accumulation] = spreader.spreadDown(Base{amount}, ladder.at(0));

            THEN("No spawn are generated")
            {
                CHECK(spawns.size() == 0);
                CHECK(accumulation == Decimal{0});
            }
        }

        WHEN("It spreads down from the second ladder stop.")
        {
            Decimal amount{1000};
            auto [spawns, accumulation] = spreader.spreadDown(Base{amount}, ladder.at(1));

            THEN("A single spawn is generated.")
            {
                REQUIRE(spreader.maxRateToProportions.size() == 1);
                REQUIRE(spreader.maxRateToProportions.at(0).second.size() >= 2);

                CHECK(spawns.size() == 1);

                THEN("This spawn cumulates all the proportions.")
                {
                    REQUIRE(spreader.maxRateToProportions.at(0).second.at(0) < Decimal{1});
                    CHECK(accumulation == amount);
                }
            }
        }
    }
}


SCENARIO("Spreader multiple proportions", "[spreaders]")
{
    GIVEN("A proportion spreader with 2 ranges of proportions")
    {
        Ladder ladder{
            1,
            2,
            4,
            8,
            16,
            32,
            64,
        };

        ProportionSpreader spreader{
            ladder,
            ProportionsMap{
                {
                    Decimal{ladder.at(4)},
                    {
                        Decimal{"0.4"},
                        Decimal{"0.6"},
                    },
                },
                {
                    Decimal{ladder.back()},
                    {
                        Decimal{"0.25"},
                        Decimal{"0.25"},
                        Decimal{"0.25"},
                        Decimal{"0.25"},
                    },
                }
            },
        };

        WHEN("It spreads at a rate below the first range limit.")
        {
            Decimal amount{10000};
            std::size_t ladderIdx = 2;
            auto [spawns, accumulation] = spreader.spreadDown(Base{amount}, ladder.at(ladderIdx));

            THEN("It spreads according to the first range")
            {
                REQUIRE(spawns.size() == 2);
                CHECK(spawns.at(0).base == amount * spreader.maxRateToProportions.at(0).second.at(0));
                CHECK(spawns.at(0).rate == ladder.at(ladderIdx - 1));
                CHECK(spawns.at(1).base == amount * spreader.maxRateToProportions.at(0).second.at(1));
                CHECK(spawns.at(1).rate == ladder.at(ladderIdx - 2));
                CHECK((spawns.at(0).base + spawns.at(1).base) == accumulation );
            }
        }

        WHEN("It spreads at the max rate of the first range.")
        {
            Decimal amount{1000};
            std::size_t ladderIdx = 4;
            auto [spawns, accumulation] = spreader.spreadDown(Base{amount}, ladder.at(ladderIdx));

            THEN("It spreads according to the first range")
            {
                REQUIRE(spawns.size() == 2);
                CHECK(spawns.at(0).base == amount * spreader.maxRateToProportions.at(0).second.at(0));
                CHECK(spawns.at(0).rate == ladder.at(ladderIdx - 1));
                CHECK(spawns.at(1).base == amount * spreader.maxRateToProportions.at(0).second.at(1));
                CHECK(spawns.at(1).rate == ladder.at(ladderIdx - 2));
                CHECK((spawns.at(0).base + spawns.at(1).base) == accumulation );
            }
        }

        WHEN("It spreads at a rate above the first range limit.")
        {
            Decimal amount{10000};
            std::size_t ladderIdx = 5;
            auto [spawns, accumulation] = spreader.spreadDown(Base{amount}, ladder.at(ladderIdx));

            THEN("It spreads according to the second range")
            {
                REQUIRE(spawns.size() == 4);
                CHECK(spawns.at(0).base == amount * spreader.maxRateToProportions.at(1).second.at(0));
                CHECK(spawns.at(0).rate == ladder.at(ladderIdx - 1));
                CHECK(spawns.at(1).base == amount * spreader.maxRateToProportions.at(1).second.at(1));
                CHECK(spawns.at(1).rate == ladder.at(ladderIdx - 2));
                CHECK(spawns.at(2).base == amount * spreader.maxRateToProportions.at(1).second.at(2));
                CHECK(spawns.at(2).rate == ladder.at(ladderIdx - 3));
                CHECK(spawns.at(3).base == amount * spreader.maxRateToProportions.at(1).second.at(3));
                CHECK(spawns.at(3).rate == ladder.at(ladderIdx - 4));
                CHECK((spawns.at(0).base + spawns.at(1).base + spawns.at(2).base + spawns.at(3).base) == accumulation );
            }
        }

    }
}