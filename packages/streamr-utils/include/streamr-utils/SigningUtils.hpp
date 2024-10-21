#ifndef STREAMR_UTILS_SIGNINGUTILS_HPP
#define STREAMR_UTILS_SIGNINGUTILS_HPP

#include <secp256k1.h>
#include <secp256k1_ecdh.h>
#include <secp256k1_recovery.h>
#include <string>
#include <cryptopp/asn.h>
#include <cryptopp/dsa.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/hex.h>
#include <cryptopp/keccak.h>
#include <cryptopp/oids.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#include "streamr-utils/BinaryUtils.hpp"

namespace streamr::utils {

using streamr::utils::BinaryUtils;

class SigningUtils {
private:
    // NOLINTNEXTLINE
    static constexpr std::string_view SIGN_MAGIC =
        "\u0019Ethereum Signed Message:\n";

public:
    static std::string createSignature(
        const std::string& payloadBinaryString, // NOLINT
        const std::string& privateKeyHex) {
        std::string hash = SigningUtils::hash(payloadBinaryString);
        std::string privateKey = BinaryUtils::hexToBinaryString(privateKeyHex);

        secp256k1_context* secpContext =
            secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
        secp256k1_ecdsa_recoverable_signature secpSignature;

        int r = secp256k1_ecdsa_sign_recoverable(
            secpContext,
            &secpSignature,
            reinterpret_cast<unsigned char*>(hash.data()),
            reinterpret_cast<unsigned char*>(privateKey.data()),
            nullptr,
            nullptr);
        int recid;
        uint8_t signatureData[65]; // NOLINT

        r = secp256k1_ecdsa_recoverable_signature_serialize_compact(
            secpContext, signatureData, &recid, &secpSignature);
        signatureData[64] = static_cast<uint8_t>(27 + recid); // NOLINT

        secp256k1_context_destroy(secpContext);
        return std::string(
            reinterpret_cast<char*>(signatureData), sizeof(signatureData));
    }

    static std::string hash(const std::string& payloadBinaryString) {
        const std::string prefixString = std::string(SIGN_MAGIC) +
            std::to_string(payloadBinaryString.length());
        std::string hash;
        CryptoPP::Keccak_256 keccak;
        CryptoPP::StringSource ss(
            prefixString + payloadBinaryString,
            true,
            new CryptoPP::HashFilter(keccak, new CryptoPP::StringSink(hash)));
        return hash;
    }
};

} // namespace streamr::utils

#endif // STREAMR_UTILS_SIGNINGUTILS_HPP
