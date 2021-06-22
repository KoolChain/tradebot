#include "catch.hpp"

#include "testnet_secrets.h"
#include "Utilities.h"

#include <binance/Time.h>

#include <tradebot/Database.h>
#include <tradebot/Exchange.h>

#include <list>


using namespace ad;
using namespace ad::tradebot;


SCENARIO("Placing orders", "[exchange]")
{
    const Pair pair{"BTC", "USDT"};
    const std::string traderName{"exchangetest"};

    GIVEN("An exchange instance.")
    {
        auto binance = Exchange{binance::Api{secret::gTestnetCredentials}};
        auto db = Database{":memory:"};

        // Sanity check
        binance.cancelAllOpenOrders(pair);
        REQUIRE(binance.listOpenOrders(pair).empty());

        Decimal averagePrice = binance.getCurrentAveragePrice(pair);

        WHEN("A buy market order is placed.")
        {
            Order immediateOrder = makeOrder(traderName, pair, Side::Buy, floor(averagePrice));
            db.insert(immediateOrder);
            binance.placeOrder(immediateOrder, Execution::Market);

            REQUIRE(immediateOrder.status == Order::Status::Active);
            REQUIRE(immediateOrder.exchangeId != -1);

            while(binance.getOrderStatus(immediateOrder) == "EXPIRED")
            {
                binance.placeOrder(immediateOrder, Execution::Market);
            }

            WHEN("The order is completed.")
            {
                THEN("The actual status of the order can be queried.")
                {
                    REQUIRE(binance.getOrderStatus(immediateOrder)
                            == "FILLED");
                }

                THEN("The complete order information can be retrieved from the exchange.")
                {
                    Json orderJson = binance.queryOrder(immediateOrder);

                    REQUIRE(orderJson["symbol"] == pair.symbol());
                    REQUIRE(orderJson["orderId"] == immediateOrder.exchangeId);
                    REQUIRE(orderJson["clientOrderId"] == immediateOrder.clientId());
                    // For a market order, the price is created at 0,
                    // and apparently not updated once the order is filled.
                    REQUIRE(jstod(orderJson["price"]) == 0.);
                    REQUIRE(jstod(orderJson["origQty"]) == immediateOrder.amount);
                    REQUIRE(jstod(orderJson["executedQty"]) == immediateOrder.amount);
                    REQUIRE(orderJson["type"] == "MARKET");
                    REQUIRE(orderJson["side"] == "BUY");
                }
            }

            // Let's "revert" the order the best we can (to conserve balance)
            {
                db.insert(immediateOrder.reverseSide());
                binance.placeOrder(immediateOrder, Execution::Market);

                while(binance.getOrderStatus(immediateOrder) == "EXPIRED")
                {
                    binance.placeOrder(immediateOrder, Execution::Market);
                }
            }
        }

        WHEN("A sell limit order is placed.")
        {
            // we want the sell to be instant, so we discount the average price
            Decimal price = floor(0.8*averagePrice);
            Order immediateOrder = makeOrder(traderName, pair, Side::Sell, price);
            db.insert(immediateOrder);
            binance.placeOrder(immediateOrder, Execution::Limit);

            REQUIRE(immediateOrder.status == Order::Status::Active);
            REQUIRE(immediateOrder.exchangeId != -1);


            auto waitStates = {"NEW", "PARTIALLY_FILLED"};
            while(std::find(waitStates.begin(), waitStates.end(),
                            binance.getOrderStatus(immediateOrder)) != waitStates.end())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds{50});
            }

            WHEN("The order is completed.")
            {
                THEN("The complete order information can be retrieved from the exchange.")
                {
                    Json orderJson = binance.queryOrder(immediateOrder);

                    REQUIRE(orderJson["status"] == "FILLED");
                    REQUIRE(orderJson["symbol"] == pair.symbol());
                    REQUIRE(orderJson["orderId"] == immediateOrder.exchangeId);
                    REQUIRE(orderJson["clientOrderId"] == immediateOrder.clientId());
                    REQUIRE(jstod(orderJson["price"]) == immediateOrder.fragmentsRate);
                    REQUIRE(jstod(orderJson["origQty"]) == immediateOrder.amount);
                    REQUIRE(jstod(orderJson["executedQty"]) == immediateOrder.amount);
                    REQUIRE(orderJson["type"] == "LIMIT");
                    REQUIRE(orderJson["side"] == "SELL");
                    REQUIRE(orderJson["timeInForce"] == "GTC");
                }
            }

            // Let's "revert" the order the best we can (to conserve balance)
            {
                db.insert(immediateOrder.reverseSide());
                binance.placeOrder(immediateOrder, Execution::Market);

                while(binance.getOrderStatus(immediateOrder) == "EXPIRED")
                {
                    binance.placeOrder(immediateOrder, Execution::Market);
                }
            }
        }

        WHEN("A buy limit FOK order is placed.")
        {
            // we want the buy to be instant, so we discount the average price
            Decimal price = floor(1.2*averagePrice);
            Order immediateOrder = makeOrder(traderName, pair, Side::Buy, price);
            db.insert(immediateOrder);
            binance.placeOrder(immediateOrder, Execution::LimitFok);

            REQUIRE(immediateOrder.status == Order::Status::Active);
            REQUIRE(immediateOrder.exchangeId != -1);

            while(binance.getOrderStatus(immediateOrder) == "EXPIRED")
            {
                binance.placeOrder(immediateOrder, Execution::LimitFok);
            }

            WHEN("The order had to complete, it could not paritally fill.")
            {
                THEN("The complete order information can be retrieved from the exchange.")
                {
                    Json orderJson = binance.queryOrder(immediateOrder);

                    REQUIRE(orderJson["status"] == "FILLED");
                    REQUIRE(orderJson["symbol"] == pair.symbol());
                    REQUIRE(orderJson["orderId"] == immediateOrder.exchangeId);
                    REQUIRE(orderJson["clientOrderId"] == immediateOrder.clientId());
                    REQUIRE(jstod(orderJson["price"]) == immediateOrder.fragmentsRate);
                    REQUIRE(jstod(orderJson["origQty"]) == immediateOrder.amount);
                    REQUIRE(jstod(orderJson["executedQty"]) == immediateOrder.amount);
                    REQUIRE(orderJson["type"] == "LIMIT");
                    REQUIRE(orderJson["side"] == "BUY");
                    REQUIRE(orderJson["timeInForce"] == "FOK");
                }
            }

            // Let's "revert" the order the best we can (to conserve balance)
            {
                revertOrder(binance, db, immediateOrder);
            }
        }

        WHEN("A buy limit FOK order is placed, with an impossible price.")
        {
            // we want the buy to be instant, so we discount the average price
            Decimal price = floor(0.5*averagePrice);
            Order impossibleOrder = makeOrder(traderName, pair, Side::Buy, price);
            db.insert(impossibleOrder);

            // A few attempts to have more chances it is not a "sporadic" expiration.
            for (int attempt = 0; attempt != 3; ++attempt)
            {
                binance.placeOrder(impossibleOrder, Execution::LimitFok);
                REQUIRE(impossibleOrder.status == Order::Status::Active);
                REQUIRE(impossibleOrder.exchangeId != -1);

                REQUIRE(binance.getOrderStatus(impossibleOrder) == "EXPIRED");
            }

            WHEN("The order is expired.")
            {
                THEN("The complete order information can be retrieved from the exchange.")
                {
                    Json orderJson = binance.queryOrder(impossibleOrder);

                    REQUIRE(orderJson["status"] == "EXPIRED");
                    REQUIRE(orderJson["symbol"] == pair.symbol());
                    REQUIRE(orderJson["orderId"] == impossibleOrder.exchangeId);
                    REQUIRE(orderJson["clientOrderId"] == impossibleOrder.clientId());
                    REQUIRE(jstod(orderJson["price"]) == impossibleOrder.fragmentsRate);
                    REQUIRE(jstod(orderJson["origQty"]) == impossibleOrder.amount);
                    REQUIRE(jstod(orderJson["executedQty"]) == Decimal{0});
                    REQUIRE(orderJson["type"] == "LIMIT");
                    REQUIRE(orderJson["side"] == "BUY");
                    REQUIRE(orderJson["timeInForce"] == "FOK");
                }
            }
        }

        WHEN("A buy market order is executed via the fill market function.")
        {
            auto before = getTimestamp();

            Order immediateOrder = makeOrder(traderName, pair, Side::Buy);
            db.insert(immediateOrder);
            std::optional<FulfilledOrder> opt = binance.fillMarketOrder(immediateOrder);
            while(!opt)
            {
                opt = binance.fillMarketOrder(immediateOrder);
            }

            Order filled = *opt;
            REQUIRE_FILLED_MARKET_ORDER(filled, Side::Buy, traderName, pair, averagePrice, before);
            REQUIRE(filled.fragmentsRate == immediateOrder.fragmentsRate);

            // "revert" the order the best we can (to conserve balance)
            {
                db.insert(immediateOrder.reverseSide());
                for(auto fill = binance.fillMarketOrder(immediateOrder);
                    !fill;
                    fill = binance.fillMarketOrder(immediateOrder))
                {}
            }
        }
    }
}


SCENARIO("Orders cancellation", "[exchange]")
{
    const Pair pair{"BTC", "USDT"};
    const std::string traderName{"exchangetest"};

    GIVEN("An exchange instance.")
    {
        auto binance = Exchange{binance::Api{secret::gTestnetCredentials}};
        auto db = Database{":memory:"};

        // Sanity check
        binance.cancelAllOpenOrders(pair);
        REQUIRE(binance.listOpenOrders(pair).empty());

        Decimal averagePrice = binance.getCurrentAveragePrice(pair);

        GIVEN("'Sending' order, never received by the exchange.")
        {
            // We use a different traderName, otherwise the order might have been
            // added and cancelled by other tests.
            Order sendingOrder{
                "NeverSendingTrader",
                pair.base,
                pair.quote,
                10.,
                1.,
                Side::Sell,
                Order::FulfillResponse::SmallSpread,
                0,
                Order::Status::Sending
            };

            db.insert(sendingOrder);

            THEN("The trader can enquire the actual status of this outstanding orders.")
            {
                REQUIRE(binance.getOrderStatus(sendingOrder)
                        == "NOTEXISTING");
            }

            THEN("The trader can try to cancel the order.")
            {
                REQUIRE_FALSE(binance.cancelOrder(sendingOrder));
            }
        }

        // Also applies to:
        // * 'cancelling' order that where not received by the exchange
        // * 'sending' orders that were received by the exchange
        GIVEN("Active order that is not fulfilled")
        {
            Order impossibleOrder{
                traderName,
                pair.base,
                pair.quote,
                0.01,
                floor(averagePrice*4),
                Side::Sell,
                Order::FulfillResponse::SmallSpread,
            };
            db.insert(impossibleOrder);
            binance.placeOrder(impossibleOrder, Execution::Limit);

            REQUIRE(impossibleOrder.status == Order::Status::Active);
            REQUIRE(impossibleOrder.exchangeId != -1);

            THEN("The trader can enquire the actual status of the order.")
            {
                REQUIRE(binance.getOrderStatus(impossibleOrder)
                        == "NEW");
            }

            THEN("The trader can cancel the order.")
            {
                REQUIRE(binance.cancelOrder(impossibleOrder));

                // The situation where a 'cancelling' order was received by the exchange
                // but a crash occured before confirmation.
                WHEN("The order is already cancelled")
                {
                    THEN("The trader can enquire the actual status of the order.")
                    {
                        REQUIRE(binance.getOrderStatus(impossibleOrder)
                                == "CANCELED");
                    }

                    THEN("The trader can cancel the order again.")
                    {
                        REQUIRE_FALSE(binance.cancelOrder(impossibleOrder));
                    }
                }
            }
        }

        // Applies to any order that is fulfilled (and for which the application was not notified)
        GIVEN("Active order")
        {
            Order immediateOrder{
                traderName,
                pair.base,
                pair.quote,
                0.001,
                floor(averagePrice),
                Side::Sell,
                Order::FulfillResponse::SmallSpread,
            };
            db.insert(immediateOrder);
            binance.placeOrder(immediateOrder, Execution::Market);

            REQUIRE(immediateOrder.status == Order::Status::Active);
            REQUIRE(immediateOrder.exchangeId != -1);

            while(binance.getOrderStatus(immediateOrder) == "EXPIRED")
            {
                binance.placeOrder(immediateOrder, Execution::Market);
            }

            WHEN("The order is completed")
            {
                THEN("The trader can enquire the actual status of the order.")
                {

                    REQUIRE(binance.getOrderStatus(immediateOrder)
                            == "FILLED");
                }

                THEN("The trader can try to cancel the order.")
                {
                    REQUIRE_FALSE(binance.cancelOrder(immediateOrder));
                }
            }

            // Let's "revert" the order the best we can (to conserve balance)
            {
                db.insert(immediateOrder.reverseSide());
                binance.placeOrder(immediateOrder, Execution::Market);

                while(binance.getOrderStatus(immediateOrder) == "EXPIRED")
                {
                    binance.placeOrder(immediateOrder, Execution::Market);
                }
            }
        }
    }
}


SCENARIO("Listing trades.", "[exchange]")
{
    const Pair pair{"BTC", "USDT"};
    const Decimal amount{"0.02"};
    const std::string traderName{"exchangetest"};

    GIVEN("An exchange instance.")
    {
        auto exchange = Exchange{binance::Api{secret::gTestnetCredentials}};
        auto db = Database{":memory:"};

        // Sanity check
        exchange.cancelAllOpenOrders(pair);
        REQUIRE(exchange.listOpenOrders(pair).empty());

        Decimal averagePrice = exchange.getCurrentAveragePrice(pair);

        WHEN("A buy market order is placed.")
        {
            Order buyLarge = makeOrder(traderName, pair, Side::Buy, Decimal{"0"}, amount);
            db.insert(buyLarge);
            fulfillMarketOrder(exchange, buyLarge);

            THEN("Its fills can be listed after the fact")
            {
                Fulfillment fulfillment = exchange.accumulateTradesFor(buyLarge, 1);

                // There were issues with the API not updating quickly enough and returning
                // a previous order in some runs. Work around it with the retrying version of
                // query order.
                Json orderJson = *exchange.tryQueryOrder(buyLarge);

                CHECK(fulfillment.amountBase == buyLarge.amount);
                CHECK(fulfillment.amountBase == jstod(orderJson.at("executedQty")));
                CHECK(fulfillment.amountQuote == jstod(orderJson.at("cummulativeQuoteQty")));
                REQUIRE(fulfillment.feeAsset != "");
                REQUIRE(fulfillment.tradeCount >= 1);

                spdlog::error("TradeCount: {}.", fulfillment.tradeCount);

                REQUIRE(fulfillment.price() != 0);
                // Another way to compute a market order price after the fact
                // see: https://dev.binance.vision/t/how-to-get-the-price-of-a-market-order-that-is-already-filled/4046/2
                REQUIRE(fulfillment.price() ==
                        jstod(orderJson.at("cummulativeQuoteQty"))/jstod(orderJson.at("executedQty")));
            }

            // "revert" the order the best we can (to conserve balance)
            revertOrder(exchange, db, buyLarge);
        }
        // TODO it would be ideal to be able to automatically test the pagination of accumulateTradesFor()
        // notably in situation where there are different trades for the order in different pages.
        // Yet it seems difficult to write a reliable test for that without a mocked-up exchange.
        // (As the test net is regularly reset, I suspect even if we hardcode the exchange id of an
        // interesting order it will later be lost).

        // Commented out because:
        // It is not automatically tested
        // The page size has to be adapted to the current total number of trades on the pair to make sense:w
        //GIVEN("A very old order.")
        //{
        //    Order oldOrder = makeOrder(traderName, pair, Side::Sell, 0., 0.);

        //    THEN("Listing trades starting from it will paginate, given small enough page size.")
        //    {
        //        Fulfillment fulfillment = exchange.accumulateTradesFor(oldOrder, 3);
        //        // Note: I don't know how to no invasively test that it did paginate...
        //        // I checked manually in the logs, and as of 2021/06/01 it did paginate.
        //    }
        //}
    }
}

SCENARIO("Listening to SPOT user data stream.")
{
    const Pair pair{"BTC", "USDT"};
    const std::string traderName{"exchangetest"};

    GIVEN("An exchange instance.")
    {
        auto exchange = Exchange{binance::Api{secret::gTestnetCredentials}};
        auto db = Database{":memory:"};

        // Sanity check
        exchange.cancelAllOpenOrders(pair);
        REQUIRE(exchange.listOpenOrders(pair).empty());

        Decimal averagePrice = exchange.getCurrentAveragePrice(pair);

        WHEN("The SPOT user stream is opened.")
        {
            std::list<Json> reports;
            auto onReport = [&](Json aReport)
            {
                reports.push_back(std::move(aReport));
            };
            REQUIRE(exchange.openUserStream(onReport));

            WHEN("A market order is placed.")
            {
                Order immediateOrder = makeOrder(traderName, pair, Side::Buy, 0, Decimal{"0.02"});
                db.insert(immediateOrder);
                exchange.placeOrder(immediateOrder, Execution::Market);

                // Hack: small delay, even more chances for the reports to be received on stream
                // (note: it was already working well without it, but with a few misses)
                std::this_thread::sleep_for(std::chrono::milliseconds{5});

                while(exchange.getOrderStatus(immediateOrder) == "EXPIRED")
                {
                    REQUIRE(reports.front().at("i") == immediateOrder.exchangeId);
                    REQUIRE(reports.front().at("x") == "NEW");
                    reports.pop_front();
                    REQUIRE(reports.front().at("i") == immediateOrder.exchangeId);
                    REQUIRE(reports.front().at("x") == "EXPIRED");
                    reports.pop_front();
                    exchange.placeOrder(immediateOrder, Execution::Market);
                }

                THEN("Execution reports are received.")
                {
                    REQUIRE(reports.front().at("i") == immediateOrder.exchangeId);
                    REQUIRE(reports.front().at("s") == immediateOrder.symbol());
                    REQUIRE(reports.front().at("c") == immediateOrder.clientId());
                    REQUIRE(jstod(reports.front().at("q")) == immediateOrder.amount);
                    REQUIRE(reports.front().at("o") == "MARKET");
                    REQUIRE(reports.front().at("x") == "NEW");
                    reports.pop_front();

                    // Remaining messages are order trade completion
                    Fulfillment fulfillment;
                    //for(const Json & report : reports)
                    std::size_t reportId = 0;
                    for(auto reportIt = reports.begin();
                        reportIt != reports.end();
                        ++reportId, ++reportIt)
                    {
                        auto report = *reportIt;
                        REQUIRE(report.at("x") == "TRADE");
                        fulfillment.accumulate(Fulfillment::fromStreamJson(report), immediateOrder);

                        if (reportId == reports.size()-1)
                        {
                            REQUIRE(report.at("X") == "FILLED");
                        }
                        else
                        {
                            REQUIRE(report.at("X") == "PARTIALLY_FILLED");
                        }
                    }

                    Json orderJson = exchange.queryOrder(immediateOrder);
                    CHECK(fulfillment.amountBase == immediateOrder.amount);
                    CHECK(fulfillment.amountBase == jstod(orderJson.at("executedQty")));
                    CHECK(fulfillment.amountQuote == jstod(orderJson.at("cummulativeQuoteQty")));
                    REQUIRE(fulfillment.feeAsset != "");
                    // Most of the times the fee is 0., I do not understand why yet.
                    // But there is a not null "commssion asset" anyway.
                    //REQUIRE(fulfillment.fee > 0.);

                    THEN("The user stream can be explicitly closed")
                    {
                        exchange.closeUserStream();
                    }
                }

                // Let's "revert" the order the best we can (to conserve balance)
                revertOrder(exchange, db, immediateOrder);
            }
        }
    }
}


SCENARIO("Retrying query order.", "[exchange]")
{
    const Pair pair{"BTC", "USDT"};
    const std::string traderName{"exchangetest"};

    GIVEN("An exchange instance.")
    {
        auto exchange = Exchange{binance::Api{secret::gTestnetCredentials}};
        auto db = Database{":memory:"};

        // Sanity check
        exchange.cancelAllOpenOrders(pair);
        REQUIRE(exchange.listOpenOrders(pair).empty());

        GIVEN("An order not present on the exchange.")
        {
            Order notInExchange = makeOrder("NeverSendingTrader", pair, Side::Sell);
            db.insert(notInExchange);

            WHEN("The order is queried via tryQueryOrder.")
            {
                auto before = getTimestamp();
                int attempts = 3;
                auto waitFor = std::chrono::milliseconds{500};
                std::optional<Json> orderJson = exchange.tryQueryOrder(notInExchange, attempts, waitFor);

                THEN("The order absence is signaled after all attempts.")
                {
                    REQUIRE_FALSE(orderJson);
                    // Note: I don't know how to properly test that the 3 attempts were made
                    // without a mocked-up exchange. So check if timing is consistent.
                    REQUIRE(getTimestamp()-before > ((attempts-1) * waitFor).count());
                }
            }
        }

        GIVEN("An order ready to be sent.")
        {
            auto before = getTimestamp();
            // The order client id is made unique, to avoid colliding with an already filled order.
            // Otherwise the already filled previous order would be found on first attempt.
            Order impossible = makeImpossibleOrder(exchange, traderName + std::to_string(before), pair);
            db.insert(impossible);

            WHEN("The order is queried via tryQueryOrder, but the order is only placed after some time.")
            {
                int attempts = 3;
                auto waitFor = std::chrono::milliseconds{500};

                std::thread sender([&]()
                {
                    std::this_thread::sleep_for(waitFor);
                    exchange.placeOrder(impossible, Execution::Limit);
                });

                std::optional<Json> orderJson = exchange.tryQueryOrder(impossible, attempts, waitFor);

                // Make sure the place order response was received before making tests.
                // Maybe this is still a race condition (on `impossible`) by the standard,
                // since there is no explicit synchronization.
                // Yet I hope it will not be the case in practice. (otherwise, add explicit sync).
                sender.join();

                THEN("The order is eventually found, after some (more) time.")
                {
                    REQUIRE(orderJson);
                    REQUIRE(impossible.exchangeId != 1); // Otherwise, did it race?
                    CHECK(orderJson->at("orderId") == impossible.exchangeId);
                    CHECK(orderJson->at("status") == "NEW");
                    // Note: I don't know how to properly test that the 3 attempts were made
                    // without a mocked-up exchange. So check if timing is consistent.
                    REQUIRE(getTimestamp()-before > waitFor.count());

                    // Let's use the opportunity to not wait for another loop.
                    // The order has been sent, so its client ID is present on the exchange.
                    GIVEN("A made-up order with same client ID as above, but not matching exchange ID.")
                    {
                        Order sameClientId = impossible;
                        ++sameClientId.exchangeId;

                        THEN("An exception will be thrown trying to query the order, after all attempts are exhausted.")
                        {
                            before = getTimestamp();
                            REQUIRE_THROWS(exchange.tryQueryOrder(sameClientId, attempts, waitFor));
                            REQUIRE(getTimestamp() > (attempts * waitFor).count());
                        }
                    }

                    WHEN("The order is queried via tryQuery order with a predicate that will not match.")
                    {
                        auto impossiblePredicate = [](const Json & aJson)
                        {
                            return aJson.at("status") == "FILLED";
                        };

                        THEN("An exception will be thrown trying to query the order, after all attempts are exhausted.")
                        {
                            before = getTimestamp();
                            REQUIRE_THROWS(exchange.tryQueryOrder(impossible,
                                                                  impossiblePredicate,
                                                                  attempts, waitFor));
                            REQUIRE(getTimestamp() > (attempts * waitFor).count());
                        }
                    }
                }

                REQUIRE(exchange.cancelOrder(impossible));
            }
        }
    }
}
