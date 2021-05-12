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
        binance::Api binance{secret::gTestnetCredentials, binance::Api::gTestNet};

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

    GIVEN("A websocket placing and order and reading single message")
    {
        static const std::string symbol = "BTCUSDT";
        binance::Api binance{secret::gTestnetCredentials, binance::Api::gTestNet};

        std::vector<std::string> received;
        net::WebSocket stream{
            [&binance]()
            {
                binance::MarketOrder order{
                    symbol,
                    binance::Side::SELL,
                    0.001,
                };
                binance.placeOrderTrade(order);
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
                spdlog::info("We done");
            }
        }

        spdlog::info("All orders for symbol {}:\n{}",
                     symbol,
                     binance.listAllOrders(symbol).json->dump(4));
    }
}
