#include "catch.hpp"

#include <binance/Decimal.h>

// TODO remove logging
#include <tradebot/Logging.h>


using namespace ad;


SCENARIO("Decimal precision tests.", "[decimal][math]")
{
    GIVEN("Two decimal values instantiated from lesser precision strings.")
    {
        Decimal a{"0.000991"};
        Decimal b{"0.000009"};

        THEN("Their addition is exact.")
        {
            Decimal expected{"0.001"};

            spdlog::trace("Adding {} and {} as {}, comparing to {}.", a, b, (a+b), expected);

            REQUIRE(Decimal(a + b) == expected);
        }
    }

    GIVEN("A decimal instantiated from floating point (with inexact representation).")
    {
        Decimal fromFloating{0.001};

        THEN("Its string output is truncated to the correct precision.")
        {
            CHECK(to_str(fromFloating) == "0.001");
            CHECK(Decimal{to_str(fromFloating)} == Decimal{"0.001"});
        }
    }
}
