#pragma once

#include "Orders.h"
#include "Json.h"

#include <websocket/WebSocket.h>

#include <chrono>
#include <optional>
#include <string>


namespace ad {
namespace binance {


using ApiKey=std::string;
using SecretKey=std::string;


struct Endpoints
{
    std::string restUrl;
    std::string websocketHost;
    std::string websocketPort;
};


struct Response
{
    long status;
    std::optional<Json> json;
};


class Api
{
public:
    Api(const Json & aSecrets, Endpoints aEndpoints);
    Api(std::istream & aSecrets, Endpoints aEndpoints);
    Api(std::istream && aSecrets, Endpoints aEndpoints);

    Response getSystemStatus();
    Response getExchangeInformation();

    Response getAllCoinsInformation();

    /// \brief Account information, including balance, permissions, commissions.
    Response getAccountInformation();

    Response createSpotListenKey();

    Response placeOrderTrade(const Order & aOrder);

    static const Endpoints gProduction;
    static const Endpoints gTestNet;

private:
    enum class Verb
    {
        GET,
        POST,
    };

    Response makeRequest(const std::string & aEndpoint);
    Response makeSignedRequest(const std::string & aEndpoint, Verb aVerb);

private:
    Endpoints mEndpoints;
    ApiKey mApiKey;
    SecretKey mSecretKey;
    std::chrono::milliseconds mReceiveWindow{1000};
};


} // namespace binance
} // namespace ad
