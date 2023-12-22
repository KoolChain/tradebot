#include "catch.hpp"

#include "testnet_secrets.h"

#include <binance/Api.h>
#include <binance/Json.h>
#include <websocket/WebSocket.h>

#include <spdlog/spdlog.h>

#include <iostream>
#include <thread>

using namespace ad;


SCENARIO("Binance REST API", "[binance][net][live]")
{
    GIVEN("A Binance interface connected to the testnet")
    {
        binance::Api binance{secret::gTestnetCredentials};

        THEN("It is possible to retrieve the account informations")
        {
            binance::Response accountInfo = binance.getAccountInformation();

            REQUIRE(accountInfo.status == 200);
            REQUIRE((*accountInfo.json)["accountType"] == "SPOT");
        }
    }
}

SCENARIO("Raw Websocket use", "[binance][net][websocket][live]")
{
    GIVEN("A websocket reading a single message")
    {
        std::vector<std::string> received;
        net::WebSocket stream{[&stream, &received](auto aMessage)
            {
                received.emplace_back(std::move(aMessage));
                stream.async_close();
            }};

        THEN("It can directly access a trade stream")
        {
            stream.run("testnet.binance.vision", "443", "/ws/btcusdt@trade");
            //stream.run("testnet.binance.vision", "443", "/ws/bnbusdt@trade");
            //stream.run("testnet.binance.vision", "443", "/ws/bnbbusd@trade");
        }

        THEN("It can subscribe to a trade stream")
        {
            Json subscription = Json::parse(R"(
            {
            "method": "SUBSCRIBE",
            "params":
            [
            "bnbbusd@trade"
            ],
            "id": 1
            }
            )");
            stream.async_send(subscription.dump());
            stream.run("testnet.binance.vision", "443", "/ws");
        }
    }

    GIVEN("A websocket placing an order and reading single message")
    {
        static const std::string symbol = "BTCUSDT";
        static const binance::ClientId clientId{"BinanceTest01"};
        binance::Api binance{secret::gTestnetCredentials};

        std::vector<std::string> received;
        net::WebSocket stream{
            [&binance]()
            {
                binance::MarketOrder order{
                    symbol,
                    binance::Side::SELL,
                    0.001,
                    binance::QuantityUnit::Base,
                    clientId,
                };
                REQUIRE(binance.placeOrderTrade(order).status == 200);
            },
            [&stream, &received](auto aMessage)
            {
                received.emplace_back(std::move(aMessage));
                stream.async_close();
            }};

        GIVEN("A SPOT listen key")
        {
            std::string listenKey = binance.createSpotListenKey().json->at("listenKey");
            REQUIRE(! listenKey.empty());

            THEN("The websocket can listen to user data stream")
            {
                std::thread t{
                    [&stream,listenKey]()
                    {
                        stream.run(binance::Api::gTestNet.websocketHost,
                                   binance::Api::gTestNet.websocketPort,
                                   "/ws/"+listenKey);
                        spdlog::info("Thread end");
                    }};
                t.join();
                // Might receive several message before the async close takes effect
                REQUIRE(received.size() >= 1);

                // Make a few checks on the received message
                Json report = Json::parse(received.at(0));
                REQUIRE(report["e"] == "executionReport");
                REQUIRE(report["s"] == symbol);
                REQUIRE(report["c"] == static_cast<const std::string &>(clientId));

                long exchangeOrderId = report["i"];

                THEN("The completed order can be found among all orders")
                {
                    binance::Response response = binance.listAllOrders(symbol);
                    REQUIRE(response.json);
                    Json allOrders = *response.json;

                    REQUIRE(allOrders.size() > 0);

                    auto count =
                        std::count_if(allOrders.begin(), allOrders.end(), [&](const auto & order)
                                      {
                                        return order["orderId"] == exchangeOrderId;
                                      });
                    REQUIRE(count == 1);

                    auto found =
                        std::find_if(allOrders.begin(), allOrders.end(), [&](const auto & order)
                                     {
                                        return order["orderId"] == exchangeOrderId;
                                     });
                    REQUIRE((*found)["clientOrderId"] == clientId);
                }
            }
        }

        // Revert the order the best we can (conserve balance)
        {
            binance::MarketOrder order{
                symbol,
                binance::Side::BUY,
                0.001,
                binance::QuantityUnit::Base,
                clientId, // we can reuse it, since the order did complete
            };
            REQUIRE(binance.placeOrderTrade(order).status == 200);
        }
    }
}
