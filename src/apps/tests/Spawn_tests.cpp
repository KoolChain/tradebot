#include "catch.hpp"

#include "Utilities.h"

#include <tradebot/Trader.h>
#include <trademath/Ladder.h>


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
