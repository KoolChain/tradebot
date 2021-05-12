#include "catch.hpp"

#include "testnet_secrets.h"

#include <binance/Api.h>

#include <tradebot/Exchange.h>
#include <tradebot/Trader.h>

#include <iostream>


using namespace ad;
using namespace ad::tradebot;


SCENARIO("Initialization clean-up", "[trader]")
{
    const Pair pair{"BTC", "USDT"};

    GIVEN("A trader instance.")
    {
        Trader trader{
            "testdog",
            pair,
            Database{":memory:"},
            Exchange{binance::Api{secret::gTestnetCredentials, binance::Api::gTestNet}}
        };

        auto & db = trader.database;
        auto & binance = trader.binance;

        WHEN("No orders are opened.")
        {
            THEN("All open orders can be cancelled anyways")
            {
                std::cout << binance.restApi.cancelAllOpenOrders(pair.symbol()).json->dump(4);
            }
        }

        WHEN("There is an opened order.")
        {
            binance::LimitOrder impossibleOrder{
                pair.symbol(),
                binance::Side::SELL,
                0.01, // qty
                100000,  // price
            };
            binance.restApi.placeOrderTrade(impossibleOrder);
            THEN("The open orders can be cancelled")
            {
                std::cout << binance.restApi.cancelAllOpenOrders(pair.symbol()).json->dump(4);
            }
        }

        GIVEN("Outsanding transitioning orders in the database.")
        {
            Order sendingOrder{
                pair.base,
                pair.quote,
                10.,
                1.,
                1.,
                Direction::Sell,
                Order::FulfillResponse::SmallSpread,
                0,
                Order::Status::Sending
            };

            db.insert(sendingOrder);

            THEN("The trader should enquire the actual status of those outstanding orders.")
            {
                REQUIRE(binance.getOrderStatus(sendingOrder, trader.name) == "NOTEXISTING");
            }
        }
    }
}
