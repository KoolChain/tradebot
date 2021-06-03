#pragma once

#include "Orders.h"
#include "Json.h"
#include "Time.h"

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
    /// \brief Tag to make a request without "body"
    /// \note The body content is currently sent as request parameters
    struct NoBody{};

    Api(const Json & aSecrets, Endpoints aEndpoints);
    Api(std::istream & aSecrets, Endpoints aEndpoints);
    Api(std::istream && aSecrets, Endpoints aEndpoints);

    Response getSystemStatus();

    Response getExchangeInformation();

    Response getAllCoinsInformation();

    /// \brief Account information, including balance, permissions, commissions.
    Response getAccountInformation();

    Response createSpotListenKey();
    // Note 2021/06/03: contrarily to the documentation, the endpoint does not accept the current key
    Response pingSpotListenKey(/*const std::string aListenKey*/);
    Response closeSpotListenKey(const std::string aListenKey);

    Response getCurrentAveragePrice(const Symbol & aSymbol);

    Response placeOrderTrade(const MarketOrder & aOrder);
    Response placeOrderTrade(const LimitOrder & aOrder);

    Response listOpenOrders(const Symbol & aSymbol);

    Response listAllOrders(const Symbol & aSymbol);

    Response listAccountTradesFromTime(const Symbol & aSymbol,
                                       MillisecondsSinceEpoch aStartTime,
                                       int aLimit=1000);
    Response listAccountTradesFromId(const Symbol & aSymbol,
                                     long aTradeId,
                                     int aLimit=1000);

    Response queryOrder(const Symbol & aSymbol, const ClientId & aClientOrderId);
    Response queryOrderForExchangeId(const Symbol & aSymbol, long aExchangeId);

    Response cancelOrder(const Symbol & aSymbol, const ClientId & aClientOrderId);
    Response cancelAllOpenOrders(const Symbol & aSymbol);

    Response getSwapHistory();

    Response getCompletedWidthdrawHistory();

    const Endpoints & getEndpoints();

    static const Endpoints gProduction;
    static const Endpoints gTestNet;

private:
    enum class Verb
    {
        DELETE,
        GET,
        POST,
        PUT,
    };

    Response makeRequest(const std::string & aEndpoint);

    /// \brief Used to configure the request
    enum class Security
    {
        None,
        ApiOnly,
        Signed,
    };

    template <class T_body = NoBody>
    Response makeRequest(Verb aVerb,
                         const std::string & aEndpoint,
                         Security aSecurity,
                         const T_body & aBody = NoBody{});

private:
    Endpoints mEndpoints;
    ApiKey mApiKey;
    SecretKey mSecretKey;
    std::chrono::milliseconds mReceiveWindow{3000};
};


} // namespace binance
} // namespace ad
