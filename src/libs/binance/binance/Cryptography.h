#pragma once


#include <string>
#include <vector>


namespace ad {
namespace crypto {


//struct Digest
//{
//    std::unique_ptr<std::byte[]> mData;
//    std::size_t mLength;
//}

/// Note: Using unsigned char only because it is what OpenSSL tends to represent bytes with.
using Digest = std::vector<unsigned char>;


Digest hashMacSha256(const std::string & aKey, const std::string & aMessage);


std::string encodeBase64(const Digest & aDigest);


std::string encodeHexadecimal(const Digest & aDigest);


} // namespace crypto
} // namespace ad
