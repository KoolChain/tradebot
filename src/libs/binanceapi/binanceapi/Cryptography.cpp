#include "Cryptography.h"

// Thank you Vinnie!
#include <boost/beast/core/detail/base64.hpp>

#include <openssl/hmac.h>

#include <array>
#include <iomanip>
#include <sstream>


namespace ad {
namespace crypto {


Digest hashMacSha256(const std::string & aKey, const std::string & aMessage)
{
    std::array<unsigned char, EVP_MAX_MD_SIZE> hash;
    unsigned int hashSize;

    HMAC(EVP_sha256(),
         aKey.data(), aKey.size(),
         reinterpret_cast<const unsigned char*>(aMessage.data()), aMessage.size(),
         hash.data(), &hashSize);

    return {hash.begin(), hash.begin()+hashSize};
}


std::string encodeBase64(const Digest & aDigest)
{
    namespace base64 = boost::beast::detail::base64;
    std::string result(base64::encoded_size(aDigest.size()), '\0');
    base64::encode(result.data(), aDigest.data(), aDigest.size());
    return result;
}


std::string encodeHexadecimal(const Digest & aDigest)
{
    std::ostringstream result;
    result << std::hex << std::setfill('0');
    // Note: std::setw is not permanent, so this approach is doomed
    //std::copy(aDigest.begin(), aDigest.end(), std::ostream_iterator<unsigned>{result});

    for (const auto & byte : aDigest)
    {
        result << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return result.str();
}


} // namespace crypto
} // namespace ad
