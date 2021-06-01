#include "catch.hpp"

#include "testnet_secrets.h"
#include "Utilities.h"

#include <binance/Time.h>

#include <tradebot/Database.h>
#include <tradebot/Exchange.h>


using namespace ad;
using namespace ad::tradebot;


SCENARIO("Placing orders", "[exchange]")
{
    const Pair pair{"BTC", "USDT"};
    const std::string traderName{"exchangetest"};

    GIVEN("An exchange instance.")
    {
        auto binance = Exchange{binance::Api{secret::gTestnetCredentials, binance::Api::gTestNet}};
        auto db = Database{":memory:"};

        // Sanity check
        binance.cancelAllOpenOrders(pair);
        REQUIRE(binance.listOpenOrders(pair).empty());

        Decimal averagePrice = binance.getCurrentAveragePrice(pair);

        WHEN("A buy market order is placed.")
        {
            Order immediateOrder = makeOrder(traderName, pair, Side::Buy, std::floor(averagePrice));
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
                    REQUIRE(std::stod(orderJson["price"].get<std::string>()) == 0.);
                    REQUIRE(std::stod(orderJson["origQty"].get<std::string>()) == immediateOrder.amount);
                    REQUIRE(std::stod(orderJson["executedQty"].get<std::string>()) == immediateOrder.amount);
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
            Decimal price = std::floor(0.8*averagePrice);
            Order immediateOrder = makeOrder(traderName, pair, Side::Sell, price);
            db.insert(immediateOrder);
            binance.placeOrder(immediateOrder, Execution::Limit);

            REQUIRE(immediateOrder.status == Order::Status::Active);
            REQUIRE(immediateOrder.exchangeId != -1);

            while(binance.getOrderStatus(immediateOrder) == "EXPIRED")
            {
                binance.placeOrder(immediateOrder, Execution::Market);
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
                    REQUIRE(std::stod(orderJson["price"].get<std::string>()) == immediateOrder.fragmentsRate);
                    REQUIRE(std::stod(orderJson["origQty"].get<std::string>()) == immediateOrder.amount);
                    REQUIRE(std::stod(orderJson["executedQty"].get<std::string>()) == immediateOrder.amount);
                    REQUIRE(orderJson["type"] == "LIMIT");
                    REQUIRE(orderJson["side"] == "SELL");
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
            REQUIRE(filled.status == Order::Status::Fulfilled);
            REQUIRE(filled.traderName == traderName);
            REQUIRE(filled.symbol() == pair.symbol());
            REQUIRE(filled.amount == immediateOrder.amount);
            REQUIRE(filled.fragmentsRate == immediateOrder.fragmentsRate);
            REQUIRE(filled.executionRate > averagePrice * 0.5);
            REQUIRE(filled.executionRate < averagePrice * 1.5);
            REQUIRE(filled.side == Side::Buy);
            REQUIRE(filled.activationTime >= before);
            REQUIRE(filled.fulfillTime >= filled.activationTime);
            REQUIRE(filled.fulfillTime <= getTimestamp());

            REQUIRE(filled.exchangeId != -1);

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
        auto binance = Exchange{binance::Api{secret::gTestnetCredentials, binance::Api::gTestNet}};
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
                std::floor(averagePrice*4),
                std::floor(averagePrice*4),
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
                0.01,
                std::floor(averagePrice),
                0.,
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
    const Pair pair{"BTC", "BUSD"};
    const Decimal amount = 0.03;
    const std::string traderName{"exchangetest"};

    GIVEN("An exchange instance.")
    {
        auto exchange = Exchange{binance::Api{secret::gTestnetCredentials, binance::Api::gTestNet}};
        auto db = Database{":memory:"};

        // Sanity check
        exchange.cancelAllOpenOrders(pair);
        REQUIRE(exchange.listOpenOrders(pair).empty());

        Decimal averagePrice = exchange.getCurrentAveragePrice(pair);

        WHEN("A buy market order is placed.")
        {
            Order buyLarge = makeOrder(traderName, pair, Side::Sell, 0, amount);
            db.insert(buyLarge);
            fulfillMarketOrder(exchange, buyLarge);

            THEN("Its fills can be listed after the fact")
            {
                Fulfillment fulfillment = exchange.accumulateTradesFor(buyLarge, 1);

                Json orderJson = exchange.queryOrder(buyLarge);

                REQUIRE(fulfillment.amountBase == buyLarge.amount);
                REQUIRE(fulfillment.amountBase == jstod(orderJson.at("executedQty")));
                REQUIRE(fulfillment.amountQuote == jstod(orderJson.at("cummulativeQuoteQty")));
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
