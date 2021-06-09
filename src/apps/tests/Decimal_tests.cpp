#include "catch.hpp"

#include <binance/Decimal.h>

// TODO remove logging
#include <tradebot/Logging.h>


using namespace ad;


SCENARIO("Output precision tests.", "[decimal]")
{
    GIVEN("A double with 9 known decimals.")
    {
        double d = 0.123456789;
        WHEN("It is output as fixed with precision 5.")
        {
            std::ostringstream oss;
            oss.precision(5);
            oss << std::fixed << d;

            THEN("The resulting string has 5 decimal places after the dot.")
            {
                // Rounding takes place
                REQUIRE(oss.str() == "0.12346");
            }
        }
    }
}


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

        THEN("Its is equal to the decimal instantiated from matching string.")
        {
            CHECK(fromFloating == Decimal{"0.001"});
        }

        THEN("Its string output is truncated to the correct precision.")
        {
            CHECK(to_str(fromFloating) == "0.00100000");
            CHECK(Decimal{to_str(fromFloating)} == Decimal{"0.001"});
        }
    }

    GIVEN("A large decimal using the full precision.")
    {
        const std::string largeString = "950000.12345678";
        Decimal largeDecimal{largeString};

        THEN("Its string representation gives back the large decimal.")
        {
            spdlog::trace("The large decimal is {}.", largeDecimal);
            CHECK(to_str(largeDecimal) == largeString);
        }
    }

    // Not sure this is relevant, or how this should really be handled at all
    //GIVEN("Two decimal values instantiated from more precise strings.")
    //{
    //    Decimal a{"0.000991000005"};
    //    Decimal b{"0.000009000005"};

    //    THEN("Their addition is exact.")
    //    {
    //        Decimal expected{"0.001"};

    //        spdlog::trace("Adding {} and {} as {}, comparing to {}.", a, b, (a+b), expected);

    //        REQUIRE(Decimal(a + b) == expected);
    //    }
    //}

    //GIVEN("Decimal values with a precision of 8.")
    //{
    //    Decimal a = 0.00099103;
    //    Decimal b = 0.00000897;

    //    THEN("Their addition is exact.")
    //    {
    //        Decimal expected = 0.001;

    //        REQUIRE(a + b == expected);
    //    }
    //}
}
