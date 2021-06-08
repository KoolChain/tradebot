#include "catch.hpp"

#include <binance/Decimal.h>

// TODO remove logging
#include <tradebot/Logging.h>


using namespace ad;


SCENARIO("Decimal precision tests.", "[decimal][math]")
{
    GIVEN("Two decimal values.")
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

    GIVEN("Decimal values with a precision of 8.")
    {
        Decimal a = 0.00099103;
        Decimal b = 0.00000897;

        THEN("Their addition is exact.")
        {
            Decimal expected = 0.001;

            REQUIRE(a + b == expected);
        }
    }
}
