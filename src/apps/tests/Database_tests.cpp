#include "catch.hpp"

#include <binance/Time.h>

#include <tradebot/Database.h>
#include <tradebot/Order.h>


using namespace ad;


SCENARIO("Testing database access.", "[db]")
{
    GIVEN("A tradebot database")
    {
        tradebot::Database db{":memory:"};

        THEN("New orders can be created")
        {
            tradebot::Order order{
                "DOGE",
                "BUSD",
                10.,
                1.4,
                1.5,
                tradebot::Direction::Sell,
                getTimestamp(),
                tradebot::Order::FulfillResponse::SmallSpread,
                tradebot::Order::Status::Cancelling,
            };

            auto id = db.insert(order);

            REQUIRE(id > 0);

            THEN("Order can be retrieved")
            {
                tradebot::Order retrieved = db.getOrder(id);

                REQUIRE(retrieved == order);
            }
        }

        THEN("New fragments can be created")
        {
            tradebot::Fragment fragment{
                "DOGE",
                "BUSD",
                5.,
                1.1,
                tradebot::Direction::Sell,
            };

            auto id = db.insert(fragment);

            REQUIRE(id > 0);

            THEN("Fragments can be retrieved")
            {
                tradebot::Fragment retrieved = db.getFragment(id);

                REQUIRE(retrieved == fragment);
            }
        }
    }
}
