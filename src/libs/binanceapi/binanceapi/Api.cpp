#include "Api.h"

#include "Cryptography.h"

#include <cpr/cpr.h>

// TODO remove
#include <iostream>


namespace ad {
namespace binanceapi {


//static const cpr::Url gBaseUrl{"https://api.binance.com"};
static const cpr::Url gBaseUrl{"https://testnet.binance.vision"};

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
            << "(" << aResponse.status_code << " " << aResponse.elapsed << "ms);\n"
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


Api::Api(const Json & aSecrets) :
        mApiKey{aSecrets["apikey"]},
        mSecretKey{aSecrets["secretkey"]}
{}


Api::Api(std::istream & aSecrets) :
        Api{[&aSecrets](){Json j; aSecrets>>j; return j;}()}
{}


Api::Api(std::istream && aSecrets) :
        Api{aSecrets}
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
    return makeSignedRequest({"/sapi/v1/capital/config/getall"});
}


Response Api::getAccountInformation()
{
    return makeSignedRequest({"/api/v3/account"});
}


Response Api::makeRequest(const std::string & aEndpoint)
{
    cpr::Response response = cpr::Get(gBaseUrl + cpr::Url{aEndpoint});
    return analyzeResponse(response);
}


Response Api::makeSignedRequest(const std::string & aEndpoint)
{
    cpr::Parameters parameters{
        {"timestamp", std::to_string(getTimestamp())},
        {"recvWindow", std::to_string(mReceiveWindow.count())},
    };
    cpr::Response response = cpr::Get(gBaseUrl + cpr::Url{aEndpoint},
                                       cpr::Header{
                                            {"X-MBX-APIKEY", mApiKey},
                                       },
                                       sign(mSecretKey, parameters));
    return analyzeResponse(response);
}


} // namespace binanceapi
} // namespace ad
