#include "catch.hpp"

#include <binance/Time.h>

#include <tradebot/Database.h>
#include <tradebot/Order.h>

#include <spdlog/spdlog.h>


using namespace ad;


SCENARIO("Order and fragment records.", "[db]")
{
    GIVEN("A tradebot database")
    {
        tradebot::Database db{":memory:"};

        THEN("New orders can be created")
        {
            tradebot::Order order{
                "dbtest",
                "DOGE",
                "BUSD",
                10.,
                1.4,
                1.5,
                tradebot::Side::Sell,
                tradebot::Order::FulfillResponse::SmallSpread,
                getTimestamp(),
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
                tradebot::Side::Sell,
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


SCENARIO("Orders selection", "[db]")
{
    GIVEN("A tradebot database")
    {
        tradebot::Database db{":memory:"};
        const tradebot::Pair pair{"DOGE", "BUSD"};

        THEN("It does not contain any order")
        {
            REQUIRE(db.selectOrders(pair, tradebot::Order::Status::Active).empty());
            REQUIRE(db.selectOrders(pair, tradebot::Order::Status::Fulfilled).empty());
            REQUIRE(db.selectOrders(pair, tradebot::Order::Status::Inactive).empty());
            REQUIRE(db.selectOrders(pair, tradebot::Order::Status::Cancelling).empty());
            REQUIRE(db.selectOrders(pair, tradebot::Order::Status::Sending).empty());
        }

        WHEN("Different orders are recorded")
        {
            const int ordersTotal = 5;
            std::vector<tradebot::Order> orders{
                ordersTotal,
                {
                    "dbtest",
                    pair.base,
                    pair.quote,
                    10.,
                    1.,
                    0.,
                    tradebot::Side::Sell,
                    tradebot::Order::FulfillResponse::SmallSpread,
                }
            };

            for (auto & order : orders)
            {
                db.insert(order);
            }

            THEN("By default orders are Inactive")
            {
                REQUIRE(db.selectOrders(pair, tradebot::Order::Status::Inactive).size() == ordersTotal);

                REQUIRE(db.selectOrders(pair, tradebot::Order::Status::Active).empty());
                REQUIRE(db.selectOrders(pair, tradebot::Order::Status::Fulfilled).empty());
                REQUIRE(db.selectOrders(pair, tradebot::Order::Status::Cancelling).empty());
                REQUIRE(db.selectOrders(pair, tradebot::Order::Status::Sending).empty());

                THEN("Orders are only selected for matching pair")
                {
                    const tradebot::Pair otherPair{"ETH", "BUSD"};
                    REQUIRE(db.selectOrders(otherPair, tradebot::Order::Status::Inactive).size() == 0);
                }
            }

            WHEN("First order is marked active, second order is marked fulfilled")
            {
                orders.at(0).status = tradebot::Order::Status::Active;
                orders.at(1).status = tradebot::Order::Status::Fulfilled;

                for (const auto & order : orders)
                {
                    db.update(order);
                }

                THEN("The selections are reflecting this update")
                {
                    REQUIRE(db.selectOrders(pair, tradebot::Order::Status::Inactive).size() == ordersTotal - 2);
                    REQUIRE(db.selectOrders(pair, tradebot::Order::Status::Active).size() == 1);
                    REQUIRE(db.selectOrders(pair, tradebot::Order::Status::Fulfilled).size() == 1);

                    REQUIRE(db.selectOrders(pair, tradebot::Order::Status::Cancelling).empty());
                    REQUIRE(db.selectOrders(pair, tradebot::Order::Status::Sending).empty());
                }
            }
        }

    }
}


SCENARIO("Order high-level creation.", "[db]")
{
    using namespace ad::tradebot;

    GIVEN("A database populated with different fragments")
    {
        Database db{":memory:"};

        Fragment baseFragment{
            "DOGE",
            "BUSD",
            10.,
            -1.,
            Side::Sell,
        };

        for(auto rate : {1., 2., 3.})
        {
            baseFragment.targetRate = rate;
            for (auto fragmentId : {1, 2})
            {
                db.insert(baseFragment);
            }
        }
        REQUIRE(db.countFragments() == 3*2);

        // Add some non-matching fragments
        {
            db.insert(Fragment{
                "USDT",
                "BUSD",
                100.,
                2.,
                Side::Sell
            });
            db.insert(Fragment{
                "DOGE",
                "USDT",
                100.,
                2.,
                Side::Sell
            });

            db.insert(Fragment{
                "DOGE",
                "BUSD",
                100.,
                1.,
                Side::Buy
            });
            db.insert(Fragment{
                "DOGE",
                "BUSD",
                100.,
                2.,
                Side::Buy
            });
            db.insert(Fragment{
                "DOGE",
                "BUSD",
                100.,
                3.,
                Side::Buy
            });
        }
        REQUIRE(db.countFragments() == 3*2 + 5);

        THEN("It is possible to retrieves all sell fragments above a threshold rate.")
        {
            std::vector<Decimal> rates = db.getSellRatesAbove(1.5, {"DOGE", "BUSD"});
            REQUIRE(rates.size() == 2);

            REQUIRE(db.getSellRatesAbove(4, {"DOGE", "BUSD"}).size() == 0);
            REQUIRE(db.getSellRatesAbove(0, {"DOGE", "BUSD"}).size() == 3);
        }

        THEN("It is possible to retrieves all buy fragments below a threshold rate.")
        {
            std::vector<Decimal> rates = db.getBuyRatesBelow(2.5, {"DOGE", "BUSD"});
            REQUIRE(rates.size() == 2);

            REQUIRE(db.getBuyRatesBelow(4, {"DOGE", "BUSD"}).size() == 3);
            REQUIRE(db.getBuyRatesBelow(0, {"DOGE", "BUSD"}).size() == 0);
        }

        THEN("Matching SELL fragments can be assigned to a new SELL order")
        {
            Order order =
                db.prepareOrder("dbtest", Side::Sell, 2., {"DOGE", "BUSD"}, Order::FulfillResponse::SmallSpread);

            REQUIRE(order.amount == 2*baseFragment.amount);
            REQUIRE(order.status == Order::Status::Inactive);

            THEN("There are no more fragments at the same rate.")
            {
                REQUIRE_THROWS(
                    db.prepareOrder("dbtest", Side::Sell, 2., {"DOGE", "BUSD"}, Order::FulfillResponse::SmallSpread));
            }

            THEN("The fragments matching the rate are now associated with the order.")
            {
                std::vector<Fragment> fragments = db.getFragmentsComposing(order);
                REQUIRE(fragments.size() == 2);

                Decimal accumulator{0.};
                for (const auto & fragment : fragments)
                {
                    REQUIRE(fragment.targetRate == 2.);
                    accumulator += fragment.amount;
                }
                REQUIRE(accumulator == order.amount);
            }
        }

        THEN("Matching BUY fragments can be assigned to a new BUY order")
        {
            Order order =
                db.prepareOrder("dbtest", Side::Buy, 2., {"DOGE", "BUSD"}, Order::FulfillResponse::SmallSpread);

            REQUIRE(order.amount == 100.);
            REQUIRE(order.status == Order::Status::Inactive);

            THEN("There are no more fragments at the same rate.")
            {
                REQUIRE_THROWS(
                    db.prepareOrder("dbtest", Side::Buy, 2., {"DOGE", "BUSD"}, Order::FulfillResponse::SmallSpread));
            }

            THEN("The fragments matching the rate are now associated with the order.")
            {
                std::vector<Fragment> fragments = db.getFragmentsComposing(order);
                REQUIRE(fragments.size() == 1);

                Decimal accumulator{0.};
                for (const auto & fragment : fragments)
                {
                    REQUIRE(fragment.targetRate == 2.);
                    accumulator += fragment.amount;
                }
                REQUIRE(accumulator == order.amount);
            }
        }
    }
}
