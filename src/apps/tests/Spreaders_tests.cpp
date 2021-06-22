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
            {
                Decimal{"0.5"},
                Decimal{"0.5"},
            },
        };

        THEN("It spreads with full precision as expected")
        {
            Decimal amount{"1000.5"};
            auto [spawns, accumulation] = spreader.spreadDown(Base{amount}, ladder.at(1));

            REQUIRE(spawns.size() == 1);
            CHECK(spawns.at(0).base == amount/2);
            CHECK(spawns.at(0).base == accumulation);

            auto [filtered, remainder] = computeTickFilter(accumulation, Decimal{1});
            CHECK(remainder == Decimal{"0.25"});


            WHEN("The spreader is given a tick size of 1.")
            {
                spreader.tickSize = Decimal{1};

                THEN("It spreads integers.")
                {
                    auto [spawns, accumulation] = spreader.spreadDown(Base{amount}, ladder.at(1));

                    REQUIRE(spawns.size() == 1);
                    CHECK(spawns.at(0).base == Decimal{500});
                    CHECK(spawns.at(0).base == accumulation);

                    auto [filtered, remainder] = computeTickFilter(accumulation, Decimal{1});
                    CHECK(remainder == Decimal{0});
                }
            }
        }
    }
}
