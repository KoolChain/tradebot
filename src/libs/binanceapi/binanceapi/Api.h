#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <optional>


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
    ApiKey mApiKey;
    SecretKey mSecretKey;
};


} // namespace binanceapi
} // namespace ad
