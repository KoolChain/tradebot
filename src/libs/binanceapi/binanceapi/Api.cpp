#include "Api.h"

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


    template <class... VT_aRequestExtra>
    Response makeRequest_impl(const cpr::Url & aEndpoint, const VT_aRequestExtra &... aExtras)
    {
        cpr::Response response = cpr::Get(gBaseUrl + aEndpoint, aExtras...);

        std::cout << response;

        if (response.status_code == 404)
        {
            std::cerr << "HTTP Response " << response.status_line;
            return Response {std::nullopt};
        }
        else if (response.status_code >= 400 && response.status_code < 500)
        {
            Json binanceError = Json::parse(response.text);
            std::cerr << "HTTP Response " << response.status_line
                      << ". Client error " << binanceError["code"] << ": "
                      << binanceError["msg"] << "\n"
                      ;
        }

        try {
            return Response{
                Json::parse(response.text)
            };
        }
        catch (const nlohmann::detail::parse_error &)
        {
            std::cerr << "Error: cannot parse response as json: '"
                << response.text << "'\n";
            return Response {std::nullopt};
        }
    }


    Response makeRequest(const cpr::Url & aEndpoint)
    {
        return makeRequest_impl(aEndpoint);
    }


    Response makeRequest(const cpr::Url & aEndpoint,
                         const ApiKey & aApiKey,
                         const SecretKey &aSecretKey)
    {
        return makeRequest_impl(aEndpoint,
                                cpr::Header{
                                    {"X-MBX-APIKEY", aApiKey},
                                });
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
    return makeRequest({"/sapi/v1/capital/config/getall"}, mApiKey, mSecretKey);
}


Response Api::getAccountInformation()
{
    return makeRequest({"/api/v3/account"}, mApiKey, mSecretKey);
}


} // namespace binanceapi
} // namespace ad
