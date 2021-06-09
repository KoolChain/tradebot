#include "catch.hpp"

#include <binance/Decimal.h>

#include <tradebot/Database.h>
#include <tradebot/Fragment.h>


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

        Decimal c{"0.001"};
        Decimal d{"0.002"};

        THEN("Their addition is exact.")
        {
            Decimal expected{"0.001"};
            REQUIRE(Decimal(a + b) == expected);

            REQUIRE(c + d == Decimal{"0.003"});
        }
    }

    GIVEN("A decimal instantiated from floating point (with inexact representation).")
    {
        Decimal fromFloating{0.001};

        // Not implemented in the current Decimal type.
        //THEN("Its is equal to the decimal instantiated from matching string.")
        //{
        //    CHECK(fromFloating == Decimal{"0.001"});
        //}

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


SCENARIO("Decimal round trip to DB.", "[decimal]")
{
    const tradebot::Pair pair{"BTC", "USDT"};

    GIVEN("A database.")
    {
        tradebot::Database db{":memory:"};

        GIVEN("Fragments stored in the DB.")
        {
            Decimal amount1{"0.001"};
            Decimal amount2{"0.002"};

            Decimal rate{"1.5"};

            tradebot::Fragment fragment1{pair.base, pair.quote, amount1, rate, tradebot::Side::Sell};
            tradebot::Fragment fragment2{pair.base, pair.quote, amount2, rate, tradebot::Side::Sell};

            auto id1 = db.insert(fragment1);
            auto id2 = db.insert(fragment2);

            WHEN("Fragments are retrieved from DB.")
            {
                tradebot::Fragment retrieved1 = db.getFragment(id1);
                tradebot::Fragment retrieved2 = db.getFragment(id2);

                THEN("Values are matching exactly.")
                {
                    CHECK(retrieved1.amount == amount1);
                    CHECK(retrieved2.amount == amount2);
                }
            }

            WHEN("All Fragments are summed.")
            {
                Decimal sum = db.sumAllFragments();

                THEN("The result is exactly equal to the sum of individual fragments.")
                {
                    CHECK(sum == amount1 + amount2);
                }
            }
        }
    }
}
