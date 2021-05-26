#include "catch.hpp"

#include "testnet_secrets.h"

#include <binance/Api.h>

#include <tradebot/Exchange.h>
#include <tradebot/Trader.h>

#include <iostream>
#include <thread>


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


SCENARIO("Orders cancellation", "[trader]")
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

        // Sanity check
        binance.cancelAllOpenOrders(pair);
        REQUIRE(binance.listOpenOrders(pair).empty());

        Decimal averagePrice = binance.getCurrentAveragePrice(pair);

        GIVEN("'Sending' order, never received by the exchange.")
        {
            Order sendingOrder{
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

            // We use a different trader name, otherwise, otherwise the order might have been
            // added and cancelled by other tests.
            const std::string otherTrader = "NeverSendingTrader";

            THEN("The trader can enquire the actual status of this outstanding orders.")
            {
                REQUIRE(binance.getOrderStatus(sendingOrder, otherTrader)
                        == "NOTEXISTING");
            }

            THEN("The trader can try to cancel the order.")
            {
                REQUIRE_FALSE(binance.cancelOrder(sendingOrder, otherTrader));
            }
        }

        // Also applies to:
        // * 'cancelling' order that where not received by the exchange
        // * 'sending' orders that were received by the exchange
        GIVEN("Active order that is not fulfilled")
        {
            Order impossibleOrder{
                pair.base,
                pair.quote,
                0.01,
                std::floor(averagePrice*4),
                std::floor(averagePrice*4),
                Side::Sell,
                Order::FulfillResponse::SmallSpread,
            };
            db.insert(impossibleOrder);
            binance.placeOrder(impossibleOrder, Execution::Limit, trader.name);

            REQUIRE(impossibleOrder.status == Order::Status::Active);
            REQUIRE(impossibleOrder.exchangeId != -1);

            THEN("The trader can enquire the actual status of the order.")
            {
                REQUIRE(binance.getOrderStatus(impossibleOrder, trader.name)
                        == "NEW");
            }

            THEN("The trader can cancel the order.")
            {
                REQUIRE(binance.cancelOrder(impossibleOrder, trader.name));

                // The situation where a 'cancelling' order was received by the exchange
                // but a crash occured before confirmation.
                WHEN("The order is already cancelled")
                {
                    THEN("The trader can enquire the actual status of the order.")
                    {
                        REQUIRE(binance.getOrderStatus(impossibleOrder, trader.name)
                                == "CANCELED");
                    }

                    THEN("The trader can cancel the order again.")
                    {
                        REQUIRE_FALSE(binance.cancelOrder(impossibleOrder, trader.name));
                    }
                }
            }
        }

        // Applies to any order that is fulfilled (and for which the application was not notified)
        GIVEN("Active order")
        {
            Order immediateOrder{
                pair.base,
                pair.quote,
                0.01,
                std::floor(averagePrice),
                0.,
                Side::Sell,
                Order::FulfillResponse::SmallSpread,
            };
            db.insert(immediateOrder);
            binance.placeOrder(immediateOrder, Execution::Market, trader.name);

            REQUIRE(immediateOrder.status == Order::Status::Active);
            REQUIRE(immediateOrder.exchangeId != -1);

            WHEN("The order is completed")
            {
                // Wait for the order to not be new anymore
                while(binance.getOrderStatus(immediateOrder, trader.name) == "NEW")
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                THEN("The trader can enquire the actual status of the order.")
                {

                    REQUIRE(binance.getOrderStatus(immediateOrder, trader.name)
                            == "FILLED");
                }

                THEN("The trader can try to cancel the order.")
                {
                    REQUIRE_FALSE(binance.cancelOrder(immediateOrder, trader.name));
                }
            }

            // Let's "revert" the order the best we can (to conserve balance)
            {
                immediateOrder.side = Side::Buy;
                db.insert(immediateOrder);
                binance.placeOrder(immediateOrder, Execution::Market, trader.name);
            }
        }
    }
}
