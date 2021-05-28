#include "catch.hpp"

#include "testnet_secrets.h"

#include <tradebot/Database.h>
#include <tradebot/Exchange.h>

#include <thread>


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
            Order immediateOrder{
                traderName,
                pair.base,
                pair.quote,
                0.01,
                std::floor(averagePrice),
                0.,
                Side::Buy,
                Order::FulfillResponse::SmallSpread,
            };
            db.insert(immediateOrder);
            binance.placeOrder(immediateOrder, Execution::Market);

            REQUIRE(immediateOrder.status == Order::Status::Active);
            REQUIRE(immediateOrder.exchangeId != -1);

            // Wait for the order to not be new anymore
            while(binance.getOrderStatus(immediateOrder) == "NEW")
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
                immediateOrder.side = Side::Sell;
                db.insert(immediateOrder);
                binance.placeOrder(immediateOrder, Execution::Market);

                while(binance.getOrderStatus(immediateOrder) != "FILLED")
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
        }

        WHEN("A sell limit order is placed.")
        {
            // we want the sell to be instant, so we discount the average price
            Decimal price = std::floor(0.8*averagePrice);
            Order immediateOrder{
                traderName,
                pair.base,
                pair.quote,
                0.001,
                price,
                0.,
                Side::Sell,
                Order::FulfillResponse::SmallSpread,
            };
            db.insert(immediateOrder);
            binance.placeOrder(immediateOrder, Execution::Limit);

            REQUIRE(immediateOrder.status == Order::Status::Active);
            REQUIRE(immediateOrder.exchangeId != -1);

            // Wait for the order to not be new anymore
            while(binance.getOrderStatus(immediateOrder) == "NEW")
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
                immediateOrder.side = Side::Buy;
                db.insert(immediateOrder);
                binance.placeOrder(immediateOrder, Execution::Market);

                while(binance.getOrderStatus(immediateOrder) != "FILLED")
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
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

            WHEN("The order is completed")
            {
                // Wait for the order to not be new anymore
                while(binance.getOrderStatus(immediateOrder) == "NEW")
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

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
                immediateOrder.side = Side::Buy;
                db.insert(immediateOrder);
                binance.placeOrder(immediateOrder, Execution::Market);

                while(binance.getOrderStatus(immediateOrder) != "FILLED")
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
        }
    }
}
