#pragma once

#include <nlohmann/json.hpp>

#include <chrono>
#include <optional>
#include <string>


namespace ad {
namespace binanceapi {


using Json = nlohmann::json;


using ApiKey=std::string;
using SecretKey=std::string;


struct Response
{
    std::optional<Json> json;
};


class Api
{
public:
    Api(const Json & aSecrets);
    Api(std::istream & aSecrets);
    Api(std::istream && aSecrets);

    static Response getSystemStatus();
    static Response getExchangeInformation();

    Response getAllCoinsInformation();

    /// \brief Acocunt information, including balance, permissions, commissions.
    Response getAccountInformation();

private:
    static Response makeRequest(const std::string & aEndpoint);
    Response makeSignedRequest(const std::string & aEndpoint);

private:
    ApiKey mApiKey;
    SecretKey mSecretKey;
    std::chrono::milliseconds mReceiveWindow{1000};
};


} // namespace binanceapi
} // namespace ad
