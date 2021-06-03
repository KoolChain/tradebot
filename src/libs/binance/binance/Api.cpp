#include "Api.h"

#include "Cryptography.h"
#include "Time.h"

#include "detail/OrdersHelpers.h"

#include <cpr/cpr.h>

#include <spdlog/spdlog.h>

// TODO remove
#include <iostream>


namespace ad {
namespace binance {

namespace detail {

cpr::Parameters initParameters(const Api::NoBody &)
{
    return {};
}

cpr::Parameters initParameters(const cpr::Parameters & aBody)
{
    return aBody;
}

} // namespace detail


//static const cpr::Url gBaseUrl{"https://api.binance.com"};
static const cpr::Url gBaseUrl{"https://testnet.binance.vision"};
static const cpr::Url gBaseWebsocketUrl{"wss://testnet.binance.vision"};
static const cpr::Url gWebsocketPort{"443"};

const Endpoints Api::gProduction{
    "https://api.binance.com",
    "stream.binance.com",
    "9443"
};

const Endpoints Api::gTestNet{
    "https://testnet.binance.vision",
    "testnet.binance.vision",
    "443"
};


namespace {

    template <class T_map>
    std::ostream & printMap(std::ostream & aOut, const T_map & aMaplike)
    {
        aOut << "{\n";
        for (const auto & item : aMaplike)
        {
            aOut << '\t' << item.first << ": " << item.second << ",\n";
        }
        return aOut << "}";
    }


    std::ostream & operator<<(std::ostream & aOut, const cpr::Header & aHeader)
    {
        return printMap(aOut, aHeader);
    }


    std::ostream & operator<<(std::ostream & aOut, const cpr::Cookies & aCookies)
    {
        return printMap(aOut, aCookies);
    }


    std::ostream & operator<<(std::ostream & aOut, const cpr::Error aError)
    {
        return aOut << aError.message << " (" << static_cast<int>(aError.code) << ")";
    }


    std::ostream & operator<<(std::ostream & aOut, const cpr::Response & aResponse)
    {
        aOut
            << "Url: " << aResponse.url << " "
            << "(" << aResponse.status_code << " " << aResponse.elapsed << "s);\n"
            << "Headers: " << aResponse.header << ";\n"
            << "Cookies: " << aResponse.cookies << ";\n"
            << "Error: " << aResponse.error << ";"
            << std::endl;

        return aOut;
    }


    cpr::Parameters & sign(const std::string & aSecretKey, cpr::Parameters & aParameters)
    {

        aParameters.Add({
            "signature",
            crypto::encodeHexadecimal(
                crypto::hashMacSha256(aSecretKey, aParameters.GetContent(cpr::CurlHolder{})))
        });
        return aParameters;
    }


    Response analyzeResponse(const std::string aVerb, const cpr::Response & aResponse)
    {
        //std::cout << aResponse;
        if (aResponse.status_code == 404)
        {
            spdlog::error("Status: {}. {} to url '{}'.",
                          aResponse.status_code,
                          aVerb,
                          std::string{aResponse.url});
            return Response {aResponse.status_code, std::nullopt};
        }
        else if (aResponse.status_code >= 400 && aResponse.status_code < 500)
        {
            Json binanceError = Json::parse(aResponse.text);
            spdlog::warn("Status: {}. {} to url '{}'. Client error {}: {}",
                         aResponse.status_code,
                         aVerb,
                         std::string{aResponse.url},
                         to_string(binanceError["code"]),
                         binanceError["msg"]);
        }
        else if (aResponse.status_code == 200)
        {
            spdlog::debug("Status: {}. {} to url '{}'.",
                          aResponse.status_code,
                          aVerb,
                          std::string{aResponse.url});
        }
        else
        {
            spdlog::critical("Unexpected status: {}. {} to url '{}'.",
                             aResponse.status_code,
                             aVerb,
                             std::string{aResponse.url});
        }

        try {
            return Response{
                aResponse.status_code,
                Json::parse(aResponse.text)
            };
        }
        catch (const nlohmann::detail::parse_error &)
        {
            spdlog::error("Error: cannot parse response as json: '{}'", aResponse.text);
            return Response {aResponse.status_code, std::nullopt};
        }
    }


} // anonymous namespace


Api::Api(const Json & aSecrets, Endpoints aEndpoints) :
        mEndpoints{std::move(aEndpoints)},
        mApiKey{aSecrets["apikey"]},
        mSecretKey{aSecrets["secretkey"]}
{}


Api::Api(std::istream & aSecrets, Endpoints aEndpoints) :
        Api{[&aSecrets](){Json j; aSecrets>>j; return j;}(), std::move(aEndpoints)}
{}


Api::Api(std::istream && aSecrets, Endpoints aEndpoints) :
        Api{aSecrets, std::move(aEndpoints)}
{}


Response Api::getSystemStatus()
{
    return makeRequest({"/sapi/v1/system/status"});
}


Response Api::getExchangeInformation()
{
    return makeRequest({"/api/v3/exchangeInfo"});
}


Response Api::getAllCoinsInformation()
{
    return makeRequest(Verb::GET, {"/sapi/v1/capital/config/getall"}, Security::Signed);
}


Response Api::getAccountInformation()
{
    return makeRequest(Verb::GET, {"/api/v3/account"}, Security::Signed);
}


Response Api::createSpotListenKey()
{
    return makeRequest(Verb::POST, {"/api/v3/userDataStream"}, Security::ApiOnly);
}


Response Api::pingSpotListenKey()
{
    return makeRequest(Verb::PUT, {"/api/v3/userDataStream"}, Security::ApiOnly);
}


Response Api::closeSpotListenKey(const std::string aListenKey)
{
    return makeRequest(Verb::DELETE, {"/api/v3/userDataStream"}, Security::ApiOnly,
                       cpr::Parameters{{"listenKey", aListenKey}});
}


Response Api::getCurrentAveragePrice(const Symbol & aSymbol)
{
    return makeRequest(Verb::GET, {"/api/v3/avgPrice"}, Security::None,
                       cpr::Parameters{{"symbol", aSymbol}});
}


Response Api::placeOrderTrade(const MarketOrder & aOrder)
{
    return makeRequest(Verb::POST, {"/api/v3/order"}, Security::Signed, aOrder);
}


Response Api::placeOrderTrade(const LimitOrder & aOrder)
{
    return makeRequest(Verb::POST, {"/api/v3/order"}, Security::Signed, aOrder);
}


Response Api::listOpenOrders(const Symbol & aSymbol)
{
    return makeRequest(Verb::GET, {"/api/v3/openOrders"}, Security::Signed,
                       cpr::Parameters{{"symbol", aSymbol}});
}


Response Api::listAllOrders(const Symbol & aSymbol)
{
    return makeRequest(Verb::GET, {"/api/v3/allOrders"}, Security::Signed,
                       cpr::Parameters{{"symbol", aSymbol}});
}


Response Api::listAccountTradesFromTime(const Symbol & aSymbol,
                                        MillisecondsSinceEpoch aStartTime,
                                        int aLimit)
{
    return makeRequest(Verb::GET, {"/api/v3/myTrades"}, Security::Signed,
                       cpr::Parameters{{"symbol", aSymbol},
                                       {"startTime", std::to_string(aStartTime)},
                                       {"limit", std::to_string(aLimit)}
                       });
}


Response Api::listAccountTradesFromId(const Symbol & aSymbol,
                                      long aTradeId,
                                      int aLimit)
{
    return makeRequest(Verb::GET, {"/api/v3/myTrades"}, Security::Signed,
                       cpr::Parameters{{"symbol", aSymbol},
                                       {"fromId", std::to_string(aTradeId)},
                                       {"limit", std::to_string(aLimit)}
                       });
}


Response Api::queryOrder(const Symbol & aSymbol, const ClientId & aClientOrderId)
{
    return makeRequest(Verb::GET, {"/api/v3/order"}, Security::Signed,
                       cpr::Parameters{{"symbol", aSymbol},
                                       {"origClientOrderId", static_cast<const std::string &>(aClientOrderId)},
                                      });
}


Response Api::queryOrderForExchangeId(const Symbol & aSymbol, long aExchangeId)
{
    return makeRequest(Verb::GET, {"/api/v3/order"}, Security::Signed,
                       cpr::Parameters{{"symbol", aSymbol},
                                       {"orderId", std::to_string(aExchangeId)},
                                      });
}


Response Api::cancelOrder(const Symbol & aSymbol, const ClientId & aClientOrderId)
{
    return makeRequest(Verb::DELETE, {"/api/v3/order"}, Security::Signed,
                       cpr::Parameters{{"symbol", aSymbol},
                                       {"origClientOrderId", static_cast<const std::string &>(aClientOrderId)}});

}


Response Api::cancelAllOpenOrders(const Symbol & aSymbol)
{
    return makeRequest(Verb::DELETE, {"/api/v3/openOrders"}, Security::Signed,
                       cpr::Parameters{{"symbol", aSymbol}});
}


Response Api::getSwapHistory()
{
    return makeRequest(Verb::GET, {"/sapi/v1/bswap/swap"}, Security::Signed,
                       cpr::Parameters{{"limit", "100"},
                                       {"status", "1"}});
}


Response Api::getCompletedWidthdrawHistory()
{
    return makeRequest(Verb::GET, {"/wapi/v3/withdrawHistory.html"}, Security::Signed,
                       cpr::Parameters{{"status", "6"} // completed
                                      });
}


const Endpoints & Api::getEndpoints()
{
    return mEndpoints;
}


Response Api::makeRequest(const std::string & aEndpoint)
{
    cpr::Response response = cpr::Get(cpr::Url{mEndpoints.restUrl} + cpr::Url{aEndpoint});
    return analyzeResponse("GET", response);
}


template <class T_body>
Response Api::makeRequest(Verb aVerb,
                          const std::string & aEndpoint,
                          Security aSecurity,
                          const T_body & aBody)
{
    cpr::Session session;

    session.SetUrl(cpr::Url{mEndpoints.restUrl} + cpr::Url{aEndpoint});

    if (aSecurity == Security::ApiOnly || aSecurity == Security::Signed)
    {
        session.SetHeader({{"X-MBX-APIKEY", mApiKey}});
    }

    cpr::Parameters parameters = detail::initParameters(aBody);
    if (aSecurity == Security::Signed)
    {
        parameters.Add({
            {"timestamp", std::to_string(getTimestamp())},
            {"recvWindow", std::to_string(mReceiveWindow.count())},
        });
        sign(mSecretKey, parameters);
    }
    session.SetParameters(parameters);

    switch (aVerb)
    {
        case Verb::DELETE:
            return analyzeResponse("DELETE", session.Delete());
        case Verb::GET:
            return analyzeResponse("GET", session.Get());
        case Verb::POST:
            return analyzeResponse("POST", session.Post());
        case Verb::PUT:
            return analyzeResponse("PUT", session.Post());
        default:
            spdlog::critical("Unhandled HTTP verb, enum value '{}'.", aVerb);
            throw std::domain_error{"Unhandled HTTP verb value."};
    }
}


} // namespace binance
} // namespace ad
