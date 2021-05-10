#include "Api.h"

#include "Cryptography.h"
#include "Time.h"

#include "detail/OrdersHelpers.h"

#include <cpr/cpr.h>

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


    Response analyzeResponse(const cpr::Response & aResponse)
    {
        std::cout << aResponse;

        if (aResponse.status_code == 404)
        {
            std::cerr << "HTTP Response " << aResponse.status_line;
            return Response {aResponse.status_code, std::nullopt};
        }
        else if (aResponse.status_code >= 400 && aResponse.status_code < 500)
        {
            Json binanceError = Json::parse(aResponse.text);
            std::cerr << "HTTP Response " << aResponse.status_line
                      << ". Client error " << binanceError["code"] << ": "
                      << binanceError["msg"] << "\n"
                      ;
        }

        try {
            return Response{
                aResponse.status_code,
                Json::parse(aResponse.text)
            };
        }
        catch (const nlohmann::detail::parse_error &)
        {
            std::cerr << "Error: cannot parse response as json: '"
                << aResponse.text << "'\n";
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


Response Api::placeOrderTrade(const MarketOrder & aOrder)
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

Response Api::makeRequest(const std::string & aEndpoint)
{
    cpr::Response response = cpr::Get(cpr::Url{mEndpoints.restUrl} + cpr::Url{aEndpoint});
    return analyzeResponse(response);
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

    cpr::Response response;
    switch (aVerb)
    {
        case Verb::GET:
            response = session.Get();
            break;
        case Verb::POST:
            response = session.Post();
            break;
    }
    return analyzeResponse(response);
}


} // namespace binance
} // namespace ad
