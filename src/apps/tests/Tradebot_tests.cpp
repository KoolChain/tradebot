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
                REQUIRE(binance.cancelAllOpenOrders(pair).size() == 0);
            }
        }

        WHEN("There is an open order.")
        {
            binance::ClientId id{"testdog01"};
            binance::LimitOrder impossibleOrder{
                pair.symbol(),
                binance::Side::SELL,
                0.01, // qty
                id,
                100000,  // price
            };
            binance.restApi.placeOrderTrade(impossibleOrder);
            THEN("The open orders can be cancelled")
            {
                std::vector<binance::ClientId> cancelled = binance.cancelAllOpenOrders(pair);
                REQUIRE(cancelled.size() == 1);
                REQUIRE(cancelled.at(0) == id);
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
