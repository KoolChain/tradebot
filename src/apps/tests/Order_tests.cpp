#include "catch.hpp"

#include <tradebot/Order.h>


using namespace ad;
using namespace ad::tradebot;


SCENARIO("Order usage.", "[order]")
{
    const Pair pair{"DOGE", "DAI"};

    GIVEN("An order")
    {
        Order order{
            "ordertest",
            pair.base,
            pair.quote,
            0.02,
            1.1,
            0.,
            Side::Buy,
        };

        REQUIRE(order.side == Side::Buy);

        THEN("Its side can be reversed.")
        {
            REQUIRE(order.reverseSide().side == Side::Sell);
            // The order is changed in place
            REQUIRE(order.side == Side::Sell);

            REQUIRE(order.reverseSide().side == Side::Buy);
            // The order is changed in place
            REQUIRE(order.side == Side::Buy);
        }
    }
}