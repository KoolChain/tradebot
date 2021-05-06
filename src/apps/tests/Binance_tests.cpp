#include "catch.hpp"

#include "testnet_secrets.h"

#include <binance/Api.h>
#include <binance/Json.h>
#include <websocket/WebSocket.h>

#include <iostream>
#include <thread>

using namespace ad;

const Json gTestnetSecrets{
    {"apikey", secret::gTestnetApiKey},
    {"secretkey", secret::gTestnetSecretKey},
};


SCENARIO("Binance REST API", "[binance][net][live]")
{
    GIVEN("A Binance interface connected to the testnet")
    {
        binance::Api binance{gTestnetSecrets, binance::Api::gTestNet};

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
    GIVEN("A websocket")
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

        GIVEN("A SPOT listen key")
        {
            binance::Api binance{gTestnetSecrets, binance::Api::gTestNet};
            std::string listenKey = binance.createSpotListenKey().json->at("listenKey");
            std::cerr << "Listenkey:" << listenKey;
            REQUIRE(! listenKey.empty());

            THEN("The websocket can listen to user data stream")
            {
                std::thread t{
                    [&stream,listenKey]()
                    {
                        stream.run(binance::Api::gTestNet.websocketHost,
                                   binance::Api::gTestNet.websocketPort,
                                   "/ws/"+listenKey);
                    }};
                t.join();
            }
        }
    }
}


    //GIVEN("A Binance interface")
    //{
    //    binance::Api binance{gTestnetSecrets};
