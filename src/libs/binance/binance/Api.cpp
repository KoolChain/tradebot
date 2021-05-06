#include "Api.h"

#include "Cryptography.h"

#include <cpr/cpr.h>

// TODO remove
#include <iostream>


namespace ad {
namespace binance {


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


    auto getTimestamp()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
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
    return makeSignedRequest({"/sapi/v1/capital/config/getall"}, Verb::GET);
}


Response Api::getAccountInformation()
{
    return makeSignedRequest({"/api/v3/account"}, Verb::GET);
}


Response Api::createSpotListenKey()
{
    return makeSignedRequest({"/api/v3/userDataStream"}, Verb::POST);
}


Response Api::makeRequest(const std::string & aEndpoint)
{
    cpr::Response response = cpr::Get(cpr::Url{mEndpoints.restUrl} + cpr::Url{aEndpoint});
    return analyzeResponse(response);
}


// TODO currently hardcoded just what I need (POST is user_stream, which does not sign)
// the makeXXXRequest methods should  be deeply refactored,
// so there is less duplication, and easier combination of:
// * Verb
// * Signed or not
// * API key or not
Response Api::makeSignedRequest(const std::string & aEndpoint, Verb aVerb)
{
    cpr::Parameters parameters{
        {"timestamp", std::to_string(getTimestamp())},
        {"recvWindow", std::to_string(mReceiveWindow.count())},
    };

    cpr::Response response;
    switch (aVerb)
    {
        case Verb::GET:
        {
            response = cpr::Get(cpr::Url{mEndpoints.restUrl} + cpr::Url{aEndpoint},
                                cpr::Header{
                                     {"X-MBX-APIKEY", mApiKey},
                                },
                                sign(mSecretKey, parameters));
            break;
        }
        case Verb::POST:
        {
            response = cpr::Post(cpr::Url{mEndpoints.restUrl} + cpr::Url{aEndpoint},
                                 cpr::Header{
                                      {"X-MBX-APIKEY", mApiKey},
                                 });
            break;
        }
    }
    return analyzeResponse(response);
}


} // namespace binance
} // namespace ad
