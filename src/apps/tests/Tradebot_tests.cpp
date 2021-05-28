#include "catch.hpp"

#include "testnet_secrets.h"

#include <binance/Api.h>
#include <binance/Time.h>

#include <tradebot/Trader.h>

#include <iostream>
#include <thread>


using namespace ad;
using namespace ad::tradebot;


SCENARIO("Radical initialization clean-up", "[trader]")
{
    const Pair pair{"BTC", "USDT"};

    GIVEN("A trader instance.")
    {
        Trader trader{
            "tradertest",
            pair,
            Database{":memory:"},
            Exchange{binance::Api{secret::gTestnetCredentials, binance::Api::gTestNet}}
        };

        auto & db = trader.database;
        auto & binance = trader.exchange;

        // Sanity check
        binance.cancelAllOpenOrders(pair);
        REQUIRE(binance.listOpenOrders(pair).empty());

        Decimal averagePrice = binance.getCurrentAveragePrice(pair);

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
                std::floor(averagePrice * 4),  // price
            };
            binance.restApi.placeOrderTrade(impossibleOrder);
            THEN("The open orders can be cancelled")
            {
                std::vector<binance::ClientId> cancelled = binance.cancelAllOpenOrders(pair);
                REQUIRE(cancelled.size() == 1);
                REQUIRE(cancelled.at(0) == id);
            }
        }
    }
}


SCENARIO("Trader low-level functions.", "[trader]")
{
    const Pair pair{"BTC", "USDT"};

    GIVEN("A trader instance.")
    {
        Trader trader{
            "tradertest",
            pair,
            Database{":memory:"},
            Exchange{binance::Api{secret::gTestnetCredentials, binance::Api::gTestNet}}
        };

        auto & db = trader.database;
        auto & exchange = trader.exchange;

        // Sanity check
        exchange.cancelAllOpenOrders(pair);
        REQUIRE(exchange.listOpenOrders(pair).empty());

        Decimal averagePrice = exchange.getCurrentAveragePrice(pair);
        Decimal impossiblePrice = std::floor(4*averagePrice);

        WHEN("Matching fragments exist in database")
        {
            db.insert(Fragment{pair.base, pair.quote, 0.001, impossiblePrice, Side::Sell});
            db.insert(Fragment{pair.base, pair.quote, 0.002, impossiblePrice, Side::Sell});

            THEN("A limit order can be placed")
            {
                Order impossible =
                    trader.placeOrderForMatchingFragments(Execution::Limit,
                                                          Side::Sell,
                                                          impossiblePrice,
                                                          Order::FulfillResponse::SmallSpread);

                // The order is recorded in database
                REQUIRE(db.getOrder(impossible.id) == impossible);

                // The order is active and matches expectations
                REQUIRE(impossible.status   == Order::Status::Active);
                REQUIRE(impossible.traderName == trader.name);
                REQUIRE(impossible.base     == pair.base);
                REQUIRE(impossible.quote    == pair.quote);
                REQUIRE(impossible.amount   == 0.001 + 0.002);
                REQUIRE(impossible.fragmentsRate == impossiblePrice);
                REQUIRE(impossible.side     == Side::Sell);
                REQUIRE(impossible.fulfillResponse == Order::FulfillResponse::SmallSpread);
                REQUIRE(impossible.exchangeId != -1);

                // The fragments are associated
                REQUIRE(db.getFragmentsComposing(impossible).size() == 2);

                THEN("This order can be queried in the exchange")
                {
                    Json orderJson = exchange.queryOrder(impossible);

                    REQUIRE(orderJson["status"] == "NEW");
                    REQUIRE(orderJson["symbol"] == impossible.symbol());
                    REQUIRE(orderJson["orderId"] == impossible.exchangeId);
                    REQUIRE(std::stod(orderJson["price"].get<std::string>()) == impossible.fragmentsRate);
                    REQUIRE(std::stod(orderJson["origQty"].get<std::string>()) == impossible.amount);
                    REQUIRE(orderJson["timeInForce"] == "GTC");
                    REQUIRE(orderJson["type"] == "LIMIT");
                    REQUIRE(orderJson["side"] == "SELL");
                    REQUIRE(orderJson["time"] == impossible.activationTime);
                }

                THEN("This order can be cancelled")
                {
                    auto backedClientId = impossible.clientId();

                    // The order is actually cancelled
                    REQUIRE(trader.cancel(impossible));

                    // Below tests duplicate some "discard tests" from Database_tests.cpp
                    REQUIRE(impossible.status == Order::Status::Inactive);
                    REQUIRE(impossible.id == -1);

                    THEN("The order is not active on the exchange anymore")
                    {
                        binance::Response response =
                            exchange.restApi.queryOrder(impossible.symbol(), backedClientId);

                        REQUIRE(response.json);
                        Json & orderJson = *response.json;

                        REQUIRE(orderJson["status"] == "CANCELED");
                        REQUIRE(orderJson["symbol"] == impossible.symbol());
                        REQUIRE(orderJson["orderId"] == impossible.exchangeId);
                        REQUIRE(orderJson["time"] == impossible.activationTime);
                    }

                }
            }
        }

        GIVEN("An order not present on the exchange")
        {
            Order absent{
                "tradertest",
                pair.base,
                pair.quote,
                0.01,
                averagePrice,
                0.,
                tradebot::Side::Sell,
                tradebot::Order::FulfillResponse::SmallSpread,
                getTimestamp(),
                tradebot::Order::Status::Sending,
            };

            THEN("It is invalid to call cancel on this order if it is not in the DB")
            {
                REQUIRE_THROWS(trader.cancel(absent));
            }

            WHEN("It is added to the database")
            {
                db.insert(absent);
                REQUIRE(absent.id != -1);

                THEN("It is valid to call cancel on this order")
                {
                    REQUIRE_FALSE(trader.cancel(absent));

                    REQUIRE(absent.status == Order::Status::Inactive);
                    REQUIRE(absent.id == -1);
                }
            }

        }
    }
}
