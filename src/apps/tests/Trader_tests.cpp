#include "catch.hpp"

#include "testnet_secrets.h"
#include "Utilities.h"

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
            Exchange{binance::Api{secret::gTestnetCredentials}}
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
                binance::OrderBase{
                    pair.symbol(),
                    binance::Side::SELL,
                    0.01, // qty
                    binance::QuantityUnit::Base,
                    id,
                },
                floor(averagePrice * 4),  // price
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


SCENARIO("Orders completion.", "[trader]")
{
    const Pair pair{"BTC", "USDT"};

    GIVEN("A trader instance.")
    {
        Trader trader{
            "tradertest",
            pair,
            Database{":memory:"},
            Exchange{binance::Api{secret::gTestnetCredentials}}
        };

        auto & db = trader.database;
        auto & exchange = trader.exchange;

        // Sanity check
        exchange.cancelAllOpenOrders(pair);
        REQUIRE(exchange.listOpenOrders(pair).empty());

        Decimal averagePrice = exchange.getCurrentAveragePrice(pair);

        THEN("It can directly fulfill a new market order.")
        {
            auto before = getTimestamp();
            Order order = makeOrder(trader.name, pair, Side::Buy);
            FulfilledOrder filled = trader.fillNewMarketOrder(order);

            REQUIRE(filled == order);
            REQUIRE_FILLED_MARKET_ORDER(order, Side::Buy, trader.name, pair, averagePrice, before);

            // Revert order
            {
                db.insert(order.reverseSide());
                trader.fillNewMarketOrder(order);
            }
        }

        GIVEN("An order filled on the exchange.")
        {
            auto before = getTimestamp();
            Order order = makeOrder(trader.name, pair, Side::Sell);
            trader.placeNewOrder(Execution::Market, order);
            auto fulfilled = exchange.fillMarketOrder(order);
            while(! (fulfilled = exchange.fillMarketOrder(order)))
            {}

            GIVEN("The corresponding fulfillment.")
            {
                Fulfillment fulfillment = exchange.accumulateTradesFor(order);

                THEN("The trader can locally complete the order.")
                {
                    trader.completeOrder(order, fulfillment);
                    REQUIRE_FILLED_MARKET_ORDER(order, Side::Sell, trader.name, pair, averagePrice, before);
                }
            }

            // Revert order
            {
                db.insert(order.reverseSide());
                trader.fillNewMarketOrder(order);
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
            Exchange{binance::Api{secret::gTestnetCredentials}}
        };

        auto & db = trader.database;
        auto & exchange = trader.exchange;

        // Sanity check
        exchange.cancelAllOpenOrders(pair);
        REQUIRE(exchange.listOpenOrders(pair).empty());

        Decimal averagePrice = exchange.getCurrentAveragePrice(pair);
        Decimal impossiblePrice = floor(4*averagePrice);

        WHEN("Matching fragments exist in database")
        {
            db.insert(Fragment{pair.base, pair.quote, fromFP(0.001), impossiblePrice, Side::Sell});
            db.insert(Fragment{pair.base, pair.quote, fromFP(0.002), impossiblePrice, Side::Sell});

            THEN("A limit order can be placed")
            {
                Order impossible =
                    trader.placeOrderForMatchingFragments(Execution::Limit,
                                                          Side::Sell,
                                                          impossiblePrice);

                // The order is recorded in database
                REQUIRE(db.getOrder(impossible.id) == impossible);

                // The order is active and matches expectations
                REQUIRE(impossible.status   == Order::Status::Active);
                REQUIRE(impossible.traderName == trader.name);
                REQUIRE(impossible.base     == pair.base);
                REQUIRE(impossible.quote    == pair.quote);
                // Is not strictly equal: the is an exact Decimal
                REQUIRE(isEqual(impossible.baseAmount, 0.001 + 0.002));
                REQUIRE(impossible.baseAmount == Decimal{"0.001"} + Decimal{"0.002"});
                REQUIRE(impossible.fragmentsRate == impossiblePrice);
                REQUIRE(impossible.side     == Side::Sell);
                REQUIRE(impossible.exchangeId != -1);

                // The fragments are associated
                REQUIRE(db.getFragmentsComposing(impossible).size() == 2);

                THEN("This order can be queried in the exchange")
                {
                    Json orderJson = *exchange.tryQueryOrder(impossible);

                    REQUIRE(orderJson["status"] == "NEW");
                    REQUIRE(orderJson["symbol"] == impossible.symbol());
                    REQUIRE(orderJson["orderId"] == impossible.exchangeId);
                    REQUIRE(jstod(orderJson["price"]) == impossible.fragmentsRate);
                    REQUIRE(jstod(orderJson["origQty"]) == impossible.baseAmount);
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
                tradebot::Side::Sell,
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
    // Make the name a bit more unique, I suspect some collisions when querying order
    // on the rest API and the exchange is not yet up to date.
    const std::string traderName = "tradertest_controlledinit";

    GIVEN("A trader instance.")
    {
        Trader trader{
            traderName,
            pair,
            Database{":memory:"},
            Exchange{binance::Api{secret::gTestnetCredentials}}
        };

        auto & db = trader.database;
        auto & exchange = trader.exchange;

        // Sanity check
        exchange.cancelAllOpenOrders(pair);
        REQUIRE(exchange.listOpenOrders(pair).empty());

        Decimal averagePrice = exchange.getCurrentAveragePrice(pair);
        Decimal impossiblePrice = floor(4*averagePrice);

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
                tradebot::Side::Sell,
            };
            db.insert(inactive);

            // Sending never received
            db.insert(Fragment{pair.base, pair.quote, 0.001, impossiblePrice, Side::Sell});
            Order sendingNeverReceived =
                db.prepareOrder(traderName, Side::Sell, impossiblePrice, pair);
            db.update(sendingNeverReceived.setStatus(Order::Status::Sending));

            // Sending and received
            db.insert(Fragment{pair.base, pair.quote, 0.001, impossiblePrice, Side::Sell});
            Order sendingAndReceived =
                trader.placeOrderForMatchingFragments(Execution::Limit,
                                                      Side::Sell,
                                                      impossiblePrice);
            db.update(sendingAndReceived.setStatus(Order::Status::Sending));

            // Active not fulfilled
            {
                db.insert(Fragment{pair.base, pair.quote, 0.001, impossiblePrice, Side::Sell});
                Order activeNotFulfilled =
                    trader.placeOrderForMatchingFragments(Execution::Limit,
                                                          Side::Sell,
                                                          impossiblePrice);
            }

            // Active fulfilled market order
            // averagePrice is only used as a marker for later test (this will be a market order)
            Fragment marketFulfilledFragment =
                db.getFragment(db.insert(Fragment{pair.base,
                                                  pair.quote,
                                                  0.001,
                                                  averagePrice,
                                                  Side::Sell}));
            Order marketFulfilled =
                trader.placeOrderForMatchingFragments(Execution::Market,
                                                      Side::Sell,
                                                      averagePrice);
            while(exchange.getOrderStatus(marketFulfilled) == "EXPIRED")
            {
                exchange.placeOrder(marketFulfilled, Execution::Market);
            }

            // Active fulfilled LIMIT order
            Decimal limitGenerous = floor(1.1 * averagePrice);
            Fragment limitFulfilledFragment =
                db.getFragment(db.insert(Fragment{pair.base,
                                                  pair.quote,
                                                  0.001,
                                                  limitGenerous,
                                                  Side::Buy}));
            Order limitFulfilled =
                trader.placeOrderForMatchingFragments(Execution::Limit,
                                                      Side::Buy,
                                                      limitGenerous);
            while(exchange.getOrderStatus(limitFulfilled) == "EXPIRED")
            {
                exchange.placeOrder(limitFulfilled, Execution::Limit);
            }
            while(exchange.getOrderStatus(limitFulfilled) == "PARTIALLY_FILLED")
            {
                std::this_thread::sleep_for(std::chrono::milliseconds{50});
            }

            // Cancelling never received
            {
                db.insert(Fragment{pair.base, pair.quote, 0.001, impossiblePrice, Side::Sell});
                Order cancelling =
                    trader.placeOrderForMatchingFragments(Execution::Limit,
                                                          Side::Sell,
                                                          impossiblePrice);
                db.update(cancelling.setStatus(Order::Status::Cancelling));
            }

            // Cancelling and received
            {
                db.insert(Fragment{pair.base, pair.quote, 0.001, impossiblePrice, Side::Sell});
                Order cancelling =
                    trader.placeOrderForMatchingFragments(Execution::Limit,
                                                          Side::Sell,
                                                          impossiblePrice);
                REQUIRE(exchange.cancelOrder(cancelling));
                db.update(cancelling.setStatus(Order::Status::Cancelling));
            }

            // Avoids the repetition...
            //THEN("All orders are found in the database")
            {
                REQUIRE(db.countFragments() == 7);
                REQUIRE(db.getUnassociatedFragments(Side::Sell, impossiblePrice, pair).empty());
                REQUIRE(db.getUnassociatedFragments(Side::Sell, averagePrice, pair).empty());

                REQUIRE(db.countOrders() == 8);

                REQUIRE(db.selectOrders(pair, Order::Status::Inactive).size() == 1);
                REQUIRE(db.selectOrders(pair, Order::Status::Sending).size() == 2);
                REQUIRE(db.selectOrders(pair, Order::Status::Active).size() == 3);
                REQUIRE(db.selectOrders(pair, Order::Status::Cancelling).size() == 2);
                REQUIRE(db.selectOrders(pair, Order::Status::Fulfilled).size() == 0);
            }

            THEN("Orders active on the exchange can be cancelled.")
            {
                REQUIRE(trader.cancelLiveOrders() == 3);

                // Only orders remaining are:
                // * inactive
                // * market active fulfilled
                // * limit active fulfilled
                REQUIRE(db.countOrders() == 3);

                REQUIRE(db.selectOrders(pair, Order::Status::Inactive).size() == 1);
                REQUIRE(db.selectOrders(pair, Order::Status::Sending).size() == 0);
                REQUIRE(db.selectOrders(pair, Order::Status::Active).size() == 0);
                REQUIRE(db.selectOrders(pair, Order::Status::Cancelling).size() == 0);
                REQUIRE(db.selectOrders(pair, Order::Status::Fulfilled).size() == 2);

                // THEN("The not fulfilled fragments are freed.")
                {
                    REQUIRE(db.countFragments() == 7);
                    REQUIRE(db.getUnassociatedFragments(Side::Sell, impossiblePrice, pair).size() == 5);
                    REQUIRE(db.getUnassociatedFragments(Side::Sell, averagePrice, pair).size() == 0);

                    REQUIRE(db.getFragmentsComposing(marketFulfilled).size() == 1);
                    REQUIRE(db.getFragmentsComposing(marketFulfilled).at(0) == db.getFragment(marketFulfilledFragment.id));
                    REQUIRE(db.getFragmentsComposing(marketFulfilled).at(0).id == marketFulfilledFragment.id);

                    REQUIRE(db.getFragmentsComposing(limitFulfilled).size() == 1);
                    REQUIRE(db.getFragmentsComposing(limitFulfilled).at(0) == db.getFragment(limitFulfilledFragment.id));
                    REQUIRE(db.getFragmentsComposing(limitFulfilled).at(0).id == limitFulfilledFragment.id);

                }

                // THEN("The fulfilled orders are marked as such.")
                {
                    db.reload(marketFulfilled);
                    REQUIRE(marketFulfilled.status == Order::Status::Fulfilled);
                    REQUIRE(marketFulfilled.executionRate > averagePrice * 0.5);
                    REQUIRE(marketFulfilled.executionRate < averagePrice * 1.5);
                    REQUIRE(marketFulfilled.fulfillTime >= marketFulfilled.activationTime);

                    db.reload(marketFulfilledFragment);
                    REQUIRE(marketFulfilledFragment.composedOrder == marketFulfilled.id);
                    REQUIRE(marketFulfilledFragment.baseAmount == marketFulfilled.baseAmount);

                    db.reload(limitFulfilled);
                    REQUIRE(limitFulfilled.status == Order::Status::Fulfilled);
                    // Note the execution rate might not be at the limit exactly it seems
                    // I suspect only if advantageous (so for a buy, we might buy at a lower rate).
                    REQUIRE(limitFulfilled.executionRate <= limitFulfilledFragment.targetRate);
                    REQUIRE(limitFulfilled.fulfillTime >= limitFulfilled.activationTime);

                    db.reload(limitFulfilledFragment);
                    REQUIRE(limitFulfilledFragment.composedOrder == limitFulfilled.id);
                    REQUIRE(limitFulfilledFragment.baseAmount == limitFulfilled.baseAmount);
                }

                THEN("There are no more orders to cancel")
                {
                    REQUIRE(trader.cancelLiveOrders() == 0);
                }
            }

            // Let's "revert" the fulfilled orders the best we can (to conserve balance)
            {
                db.insert(marketFulfilled.reverseSide());
                exchange.placeOrder(marketFulfilled, Execution::Market);

                while(exchange.getOrderStatus(marketFulfilled) == "EXPIRED")
                {
                    exchange.placeOrder(marketFulfilled, Execution::Market);
                }

                db.insert(limitFulfilled.reverseSide());
                exchange.placeOrder(limitFulfilled, Execution::Market);

                while(exchange.getOrderStatus(limitFulfilled) == "EXPIRED")
                {
                    exchange.placeOrder(limitFulfilled, Execution::Market);
                }
            }
        }
    }
}


Decimal filterPrice(Decimal aPrice, SymbolFilters aFilters)
{
    return trade::applyTickSizeFloor(aPrice, aFilters.price.tickSize);
}


Decimal computeMinimalAmount(Decimal aPrice, SymbolFilters aFilters)
{
    Decimal quantity = aFilters.minimumNotional / aPrice;
    Decimal remainder = 0;
    std::tie(quantity, remainder) = trade::computeTickFilter(quantity, aFilters.amount.tickSize);
    if(remainder != 0)
    {
        assert(quantity * aPrice < aFilters.minimumNotional);
        quantity += aFilters.amount.tickSize;
        assert(quantity * aPrice > aFilters.minimumNotional);
    }
    else
    {
        assert(quantity * aPrice == aFilters.minimumNotional);
    }
    assert(quantity >= aFilters.amount.minimum && quantity <= aFilters.amount.maximum);
    return quantity;
}


SCENARIO("Fill profitable orders.", "[trader]")
{
    const Pair pair{"BTC", "USDT"};

    GIVEN("A trader instance.")
    {
        Trader trader{
            "tradertest",
            pair,
            Database{":memory:"},
            Exchange{binance::Api{secret::gTestnetCredentials}}
        };

        auto & db = trader.database;
        auto & binance = trader.exchange;

        SymbolFilters symbolFilters = trader.queryFilters();

        // Sanity check
        binance.cancelAllOpenOrders(pair);
        REQUIRE(binance.listOpenOrders(pair).empty());

        Decimal averagePrice = binance.getCurrentAveragePrice(pair);
        INFO("The average price is " << averagePrice);

        WHEN("3 sell fragments at 2 different rates below average are written to DB.")
        {
            const Decimal filteredOrderPrice = filterPrice(averagePrice / 1.1, symbolFilters);
            Decimal amount = computeMinimalAmount(filteredOrderPrice, symbolFilters);

            Fragment f{pair.base, pair.quote, amount, filterPrice(averagePrice/1.2, symbolFilters), Side::Sell};
            db.insert(f);
            db.insert(f);
            f.targetRate = filterPrice(averagePrice/1.3, symbolFilters);
            db.insert(f);

            // Sanity check
            REQUIRE(db.getUnassociatedFragments(Side::Sell, pair).size() == 3);
            REQUIRE(db.selectOrders(pair, Order::Status::Fulfilled).size() == 0);

            THEN("They can be filled at once as profitable orders.")
            {
                auto [sellCount, buyCount] =
                    trader.makeAndFillProfitableOrders({filteredOrderPrice, filteredOrderPrice}, symbolFilters);

                REQUIRE(sellCount == 2);
                REQUIRE(buyCount == 0);

                REQUIRE(db.getUnassociatedFragments(Side::Sell, pair).size() == 0);
                REQUIRE(db.selectOrders(pair, Order::Status::Fulfilled).size() == 2);
            }
        }

        WHEN("3 buy fragments at 3 different rates above average are written to DB.")
        {
            const Decimal filteredOrderPrice = filterPrice(averagePrice * 1.1, symbolFilters);
            Decimal amount = computeMinimalAmount(filteredOrderPrice, symbolFilters);

            Fragment f{pair.base, pair.quote, amount, filterPrice(averagePrice*1.2, symbolFilters), Side::Buy};
            db.insert(f);
            f.targetRate = filterPrice(averagePrice*1.3, symbolFilters);
            db.insert(f);
            f.targetRate = filterPrice(averagePrice*1.4, symbolFilters);
            db.insert(f);

            // Sanity check
            REQUIRE(db.getUnassociatedFragments(Side::Buy, pair).size() == 3);
            REQUIRE(db.selectOrders(pair, Order::Status::Fulfilled).size() == 0);

            THEN("They can be filled at once as profitable orders.")
            {
                auto [sellCount, buyCount] =
                    trader.makeAndFillProfitableOrders({filteredOrderPrice, filteredOrderPrice}, symbolFilters);

                REQUIRE(sellCount == 0);
                REQUIRE(buyCount == 3);

                REQUIRE(db.getUnassociatedFragments(Side::Buy, pair).size() == 0);
                REQUIRE(db.selectOrders(pair, Order::Status::Fulfilled).size() == 3);
            }
        }
    }
}


SCENARIO("Fill profitable orders limit bug.", "[trader][bug]")
{
    // The bug: Trader::makeAndFillProfitableOrders is sending limit fok orders at the fragments
    // rate, instead of one of the edge rate of the provided interval.
    GIVEN("A trader instance.")
    {
        const Pair pair{"BTC", "USDT"};
        Trader trader{
            "tradertest",
            pair,
            Database{":memory:"},
            Exchange{binance::Api{secret::gTestnetCredentials}}
        };

        auto & db = trader.database;
        auto & exchange = trader.exchange;
        SymbolFilters symbolFilters = trader.queryFilters();

        // Sanity check
        exchange.cancelAllOpenOrders(pair);
        REQUIRE(exchange.listOpenOrders(pair).empty());

        Decimal averagePrice = exchange.getCurrentAveragePrice(pair);
        INFO("The average price is " << averagePrice);

        WHEN("A sell fragments at rates much below is written to DB (it would trigger price filter).")
        {
            Decimal amount{"0.01"};
            Decimal offendingLowPrice = filterPrice(averagePrice/10, symbolFilters);
            Fragment f{pair.base, pair.quote, amount, offendingLowPrice, Side::Sell};
            db.insert(f);

            // Sanity check
            REQUIRE(db.getUnassociatedFragments(Side::Sell, pair).size() == 1);
            REQUIRE(db.selectOrders(pair, Order::Status::Fulfilled).size() == 0);

            GIVEN("A price interval satisfying the price filters.")
            {
                Interval acceptableRange{
                    trade::applyTickSizeFloor(averagePrice/1.5, symbolFilters.price.tickSize),
                    trade::applyTickSizeFloor(averagePrice*1.5, symbolFilters.price.tickSize)
                };

                THEN("A profitable order can be filled within this valid range, despite the invalid low fragment price.")
                {
                    // If the operation place the order at fragments rate, it will throw when placing
                    // the order because the fragment rate will not be accepted by price filters.
                    auto [sells, buys] =
                        trader.makeAndFillProfitableOrders(acceptableRange, symbolFilters);
                    REQUIRE(sells == 1);
                    REQUIRE(buys == 0);

                    REQUIRE(db.getUnassociatedFragments(Side::Sell, pair).size() == 0);
                    REQUIRE(db.selectOrders(pair, Order::Status::Fulfilled).size() == 1);

                    Order filledOrder = db.selectOrders(pair, Order::Status::Fulfilled).at(0);

                    CHECK(filledOrder.baseAmount == amount);
                    CHECK(filledOrder.fragmentsRate == offendingLowPrice);
                    // Ideally, we would like to check the actual limit price sent to the exchange.
                    // Yet this is difficult without a mocked up exchange.
                    CHECK(filledOrder.executionRate >= acceptableRange.front);

                    revertOrder(exchange, db, makeOrder(trader.name, pair, f.side, Decimal{0}, f.baseAmount));
                }
            }
        }
    }
}


SCENARIO("Symbol filters", "[trader]")
{
    GIVEN("A trader instance and a pair.")
    {
        const Pair pair{"BTC", "USDT"};

        Trader trader{
            "tradertest",
            pair,
            Database{":memory:"},
            Exchange{binance::Api{secret::gTestnetCredentials}}
        };

        THEN("The filters for the pair can be retrieved.")
        {
            SymbolFilters filters = trader.queryFilters();

            // Expected filters at the time this test is implemented 2023/12/21: 
            //"filters": [
            //    {
            //        "filterType":"PRICE_FILTER",
            //        "maxPrice":"1000000.00000000",
            //        "minPrice":"0.01000000",
            //        "tickSize":"0.01000000"
            //    },
            //    {
            //        "filterType":"LOT_SIZE",
            //        "maxQty":"9000.00000000",
            //        "minQty":"0.00001000",
            //        "stepSize":"0.00001000"
            //    },
            //    {
            //        "filterType":"ICEBERG_PARTS",
            //        "limit":10
            //    },
            //    {
            //        "filterType":"MARKET_LOT_SIZE",
            //        "maxQty":"113.82525454",
            //        "minQty":"0.00000000",
            //        "stepSize":"0.00000000"
            //    },
            //    {
            //        "filterType":"TRAILING_DELTA",
            //        "maxTrailingAboveDelta":2000,
            //        "maxTrailingBelowDelta":2000,
            //        "minTrailingAboveDelta":10,
            //        "minTrailingBelowDelta":10
            //    },
            //    {
            //        "askMultiplierDown":"0.2",
            //        "askMultiplierUp":"5",
            //        "avgPriceMins":5,
            //        "bidMultiplierDown":"0.2",
            //        "bidMultiplierUp":"5",
            //        "filterType":"PERCENT_PRICE_BY_SIDE"
            //    },
            //    {
            //        "applyMaxToMarket":false,
            //        "applyMinToMarket":true,
            //        "avgPriceMins":5,
            //        "filterType":"NOTIONAL",
            //        "maxNotional":"9000000.00000000",
            //        "minNotional":"5.00000000"
            //    },
            //    {
            //        "filterType":"MAX_NUM_ORDERS",
            //        "maxNumOrders":200
            //    },
            //    {
            //        "filterType":"MAX_NUM_ALGO_ORDERS",
            //        "maxNumAlgoOrders":5
            //    }
            //]
            CHECK(filters.price.minimum == Decimal{"0.01"});
            CHECK(filters.price.maximum == Decimal{"1000000.00000000"});
            CHECK(filters.price.tickSize == Decimal{"0.01"});

            CHECK(filters.amount.minimum == Decimal{"0.00001"});
            CHECK(filters.amount.maximum == Decimal{"9000"});
            CHECK(filters.amount.tickSize == Decimal{"0.00001"});

            CHECK(filters.minimumNotional == Decimal{"5"});
        }
    }
}
