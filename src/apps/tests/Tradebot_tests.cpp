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
                    REQUIRE(jstod(orderJson["price"]) == impossible.fragmentsRate);
                    REQUIRE(jstod(orderJson["origQty"]) == impossible.amount);
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


SCENARIO("Controlled initialization clean-up", "[trader]")
{
    const Pair pair{"BTC", "USDT"};
    const std::string traderName = "tradertest";

    GIVEN("A trader instance.")
    {
        Trader trader{
            traderName,
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

        WHEN("No orders are present.")
        {
            THEN("The initial 'cancel live orders' can still be invoked")
            {
                REQUIRE(trader.cancelLiveOrders() == 0);
            }
        }

        GIVEN("One order in each possible state")
        {
            // Inactive never sent
            Order inactive{
                traderName,
                pair.base,
                pair.quote,
                0.01,
                averagePrice,
                0.,
                tradebot::Side::Sell,
                tradebot::Order::FulfillResponse::SmallSpread,
            };
            db.insert(inactive);

            // Sending never received
            db.insert(Fragment{pair.base, pair.quote, 0.001, impossiblePrice, Side::Sell});
            Order sendingNeverReceived =
                db.prepareOrder(traderName, Side::Sell, impossiblePrice, pair,
                                Order::FulfillResponse::SmallSpread);
            db.update(sendingNeverReceived.setStatus(Order::Status::Sending));

            // Sending and received
            db.insert(Fragment{pair.base, pair.quote, 0.001, impossiblePrice, Side::Sell});
            Order sendingAndReceived =
                trader.placeOrderForMatchingFragments(Execution::Limit,
                                                      Side::Sell,
                                                      impossiblePrice,
                                                      Order::FulfillResponse::SmallSpread);
            db.update(sendingAndReceived.setStatus(Order::Status::Sending));

            // Active not fulfilled
            {
                db.insert(Fragment{pair.base, pair.quote, 0.001, impossiblePrice, Side::Sell});
                Order activeNotFulfilled =
                    trader.placeOrderForMatchingFragments(Execution::Limit,
                                                          Side::Sell,
                                                          impossiblePrice,
                                                          Order::FulfillResponse::SmallSpread);
            }

            // TODO Reactivate this test for market orders
            // Also reactive the counter order...
            // when we have a way to retrieve the price of the order after the fact
            //// Active fulfilled market order
            //// averagePrice is only used as a marker for later test (this will be a market order)
            //Fragment fulfilledFragment =
            //    db.getFragment(db.insert(Fragment{pair.base,
            //                                      pair.quote,
            //                                      0.001,
            //                                      averagePrice,
            //                                      Side::Sell}));
            //Order activeFulfilled =
            //    trader.placeOrderForMatchingFragments(Execution::Market,
            //                                          Side::Sell,
            //                                          averagePrice,
            //                                          Order::FulfillResponse::SmallSpread);
            //while(exchange.getOrderStatus(activeFulfilled) != "FILLED")
            //{
            //    // TODO remove
            //    spdlog::trace("Status: {}",exchange.getOrderStatus(activeFulfilled));
            //    std::this_thread::sleep_for(std::chrono::milliseconds(50));
            //}

            // Active fulfilled LIMIT order
            Decimal limitGenerous = std::floor(1.1 * averagePrice);
            Fragment limitFulfilledFragment =
                db.getFragment(db.insert(Fragment{pair.base,
                                                  pair.quote,
                                                  0.001,
                                                  limitGenerous,
                                                  Side::Buy}));
            Order limitFulfilled =
                trader.placeOrderForMatchingFragments(Execution::Limit,
                                                      Side::Buy,
                                                      limitGenerous,
                                                      Order::FulfillResponse::SmallSpread);
            while(exchange.getOrderStatus(limitFulfilled) != "FILLED")
            {
                // TODO remove
                spdlog::trace("Status: {}",exchange.getOrderStatus(limitFulfilled));
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            // Cancelling never received
            {
                db.insert(Fragment{pair.base, pair.quote, 0.001, impossiblePrice, Side::Sell});
                Order cancelling =
                    trader.placeOrderForMatchingFragments(Execution::Limit,
                                                          Side::Sell,
                                                          impossiblePrice,
                                                          Order::FulfillResponse::SmallSpread);
                db.update(cancelling.setStatus(Order::Status::Cancelling));
            }

            // Cancelling and received
            {
                db.insert(Fragment{pair.base, pair.quote, 0.001, impossiblePrice, Side::Sell});
                Order cancelling =
                    trader.placeOrderForMatchingFragments(Execution::Limit,
                                                          Side::Sell,
                                                          impossiblePrice,
                                                          Order::FulfillResponse::SmallSpread);
                REQUIRE(exchange.cancelOrder(cancelling));
                db.update(cancelling.setStatus(Order::Status::Cancelling));
            }

            // Avoids the repetition...
            //THEN("All orders are found in the database")
            {
                REQUIRE(db.countFragments() == 6);
                REQUIRE(db.getUnassociatedFragments(Side::Sell, impossiblePrice, pair).empty());
                REQUIRE(db.getUnassociatedFragments(Side::Sell, averagePrice, pair).empty());

                REQUIRE(db.countOrders() == 7);

                REQUIRE(db.selectOrders(pair, Order::Status::Inactive).size() == 1);
                REQUIRE(db.selectOrders(pair, Order::Status::Sending).size() == 2);
                REQUIRE(db.selectOrders(pair, Order::Status::Active).size() == 2);
                REQUIRE(db.selectOrders(pair, Order::Status::Cancelling).size() == 2);
                REQUIRE(db.selectOrders(pair, Order::Status::Fulfilled).size() == 0);
            }

            THEN("Orders active on the exchange can be cancelled.")
            {
                REQUIRE(trader.cancelLiveOrders() == 3);

                // Only orders remaining are:
                // * inactive
                // * active fulfilled
                REQUIRE(db.countOrders() == 2);

                REQUIRE(db.selectOrders(pair, Order::Status::Inactive).size() == 1);
                REQUIRE(db.selectOrders(pair, Order::Status::Sending).size() == 0);
                REQUIRE(db.selectOrders(pair, Order::Status::Active).size() == 0);
                REQUIRE(db.selectOrders(pair, Order::Status::Cancelling).size() == 0);
                REQUIRE(db.selectOrders(pair, Order::Status::Fulfilled).size() == 1);

                // THEN("The not fulfilled fragments are freed.")
                {
                    REQUIRE(db.countFragments() == 6);
                    REQUIRE(db.getUnassociatedFragments(Side::Sell, impossiblePrice, pair).size() == 5);
                    REQUIRE(db.getUnassociatedFragments(Side::Sell, averagePrice, pair).size() == 0);

                    // TODO re-activate when the market order above is re-activated
                    //REQUIRE(db.getFragmentsComposing(activeFulfilled).size() == 1);
                    //REQUIRE(db.getFragmentsComposing(activeFulfilled).at(0) == db.getFragment(fulfilledFragment.id));
                    //REQUIRE(db.getFragmentsComposing(activeFulfilled).at(0).id == fulfilledFragment.id);

                    REQUIRE(db.getFragmentsComposing(limitFulfilled).size() == 1);
                    REQUIRE(db.getFragmentsComposing(limitFulfilled).at(0) == db.getFragment(limitFulfilledFragment.id));
                    REQUIRE(db.getFragmentsComposing(limitFulfilled).at(0).id == limitFulfilledFragment.id);

                }

                // THEN("The fulfilled orders are marked as such.")
                {
                    db.reload(limitFulfilled);
                    REQUIRE(limitFulfilled.status == Order::Status::Fulfilled);
                    REQUIRE(limitFulfilled.executionRate == limitFulfilledFragment.targetRate);
                    REQUIRE(limitFulfilled.fulfillTime >= limitFulfilled.activationTime);

                    db.reload(limitFulfilledFragment);
                    REQUIRE(limitFulfilledFragment.composedOrder == limitFulfilled.id);
                    REQUIRE(limitFulfilledFragment.amount == limitFulfilled.amount);
                }

                THEN("There are no more orders to cancel")
                {
                    REQUIRE(trader.cancelLiveOrders() == 0);
                }
            }

            // Let's "revert" the fulfilled orders the best we can (to conserve balance)
            {
                // TODO re-activate when the market order above is re-activated
                //activeFulfilled.side = Side::Buy;
                //db.insert(activeFulfilled);
                //exchange.placeOrder(activeFulfilled, Execution::Market);
                //
                //while(exchange.getOrderStatus(activeFulfilled) != "FILLED")
                //{
                //    // TODO remove
                //    spdlog::trace("Status: {}",exchange.getOrderStatus(activeFulfilled));
                //    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                //}
                //
                db.insert(limitFulfilled.reverseSide());
                exchange.placeOrder(limitFulfilled, Execution::Market);

                //TODO replace
                while(exchange.getOrderStatus(limitFulfilled) != "FILLED")
                {
                    // TODO remove
                    spdlog::trace("Status: {}",exchange.getOrderStatus(limitFulfilled));
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
        }
    }
}

// TODO ensure the active order that fulfilled are marked as such after initialization
