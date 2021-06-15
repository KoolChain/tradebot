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
            Decimal{"0.02"},
            Decimal{"1.1"},
            Side::Buy,
        };

        REQUIRE(order.side == Side::Buy);
        REQUIRE(order.amount == Decimal{"0.02"});


        THEN("Its side can be reversed.")
        {
            REQUIRE(order.reverseSide().side == Side::Sell);
            // The order is changed in place
            REQUIRE(order.side == Side::Sell);

            REQUIRE(order.reverseSide().side == Side::Buy);
            // The order is changed in place
            REQUIRE(order.side == Side::Buy);
        }


        THEN("Its symbol can be obtained.")
        {
            CHECK(order.symbol() == "DOGEDAI");
        }


        WHEN("The order has an execution rate.")
        {
            order.status = Order::Status::Fulfilled;
            THEN("Its execution quote amount can be obtained.")
            {
                order.executionRate = Decimal(3);
                CHECK(order.executionQuoteAmount() == order.amount * 3);
            }

            THEN("Its execution quote amount can be obtained.")
            {
                order.executionRate = Decimal("0.02");
                CHECK(order.executionQuoteAmount() == order.amount * Decimal{"0.02"});
            }
        }
    }
}
