#ifndef CERTIFICATE_HELPER_HPP
#define CERTIFICATE_HELPER_HPP

#include <memory>
#include <string>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

namespace streamr::dht::utils {

struct TlsCertificate {
    std::string privateKey;
    std::string cert;
};

constexpr int rsaKeyLength = 2048;
using BIO_ptr = std::unique_ptr<BIO, decltype(&BIO_free)>;
using X509_ptr = std::unique_ptr<X509, decltype(&X509_free)>;
using ASN1_TIME_ptr = std::unique_ptr<ASN1_TIME, decltype(&ASN1_STRING_free)>;

class CertificateHelper {
public:
    static std::string bioToString(
        const BIO_ptr& bio, const int& max_len) { // NOLINT
        // We are careful to do operations based on explicit lengths, not
        // depending on null terminated character streams except where we ensure
        // the terminator

        // Create a buffer and zero it out
        char buffer[max_len]; // NOLINT
        memset(buffer, 0, max_len);
        // Read one smaller than the buffer to make sure we end up with a null
        // terminator no matter what
        BIO_read(bio.get(), buffer, max_len - 1);
        return std::string(buffer);
    }

// The code below uses the deprecated OpenSSL functions
// beacuse example code was easier to find for the old API.

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

    static TlsCertificate createSelfSignedCertificate(int64_t daysValid) {
        bool result = false;

        std::unique_ptr<RSA, void (*)(RSA*)> rsa{RSA_new(), RSA_free}; // NOLINT
        std::unique_ptr<BIGNUM, void (*)(BIGNUM*)> bn{BN_new(), BN_free};

        BN_set_word(bn.get(), RSA_F4);
        int rsaOk = RSA_generate_key_ex( // NOLINT
            rsa.get(),
            rsaKeyLength,
            bn.get(),
            nullptr);

        // --- cert generation ---
        std::unique_ptr<X509, void (*)(X509*)> cert{X509_new(), X509_free};
        std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY*)> pkey{
            EVP_PKEY_new(), EVP_PKEY_free};

        // The RSA structure will be automatically freed when the
        // EVP_PKEY structure is freed.
        EVP_PKEY_assign( // NOLINT
            pkey.get(),
            EVP_PKEY_RSA,
            reinterpret_cast<char*>(rsa.release()));
        ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1); // serial number

        X509_gmtime_adj(X509_get_notBefore(cert.get()), 0); // now
        X509_gmtime_adj(
            X509_get_notAfter(cert.get()),
            daysValid * 24 * 3600); // NOLINT

        X509_set_pubkey(cert.get(), pkey.get());

        // 1 -- X509_NAME may disambig with wincrypt.h
        // 2 -- DO NO FREE the name internal pointer
        X509_name_st* name = X509_get_subject_name(cert.get());

        const unsigned char country[] = "RU"; // NOLINT
        const unsigned char company[] = "MyCompany, PLC"; // NOLINT
        const unsigned char common_name[] = "localhost"; // NOLINT

        X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, country, -1, -1, 0);
        X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, company, -1, -1, 0);
        X509_NAME_add_entry_by_txt(
            name, "CN", MBSTRING_ASC, common_name, -1, -1, 0);

        X509_set_issuer_name(cert.get(), name);
        X509_sign(cert.get(), pkey.get(),
                  EVP_sha256()); // some hash type here

        BIO_ptr keyOutputBio(BIO_new(BIO_s_mem()), BIO_free);

        int ret = PEM_write_bio_PrivateKey(
            keyOutputBio.get(),
            pkey.get(),
            nullptr,
            nullptr,
            0,
            nullptr,
            nullptr);
        std::string keyString = bioToString(keyOutputBio, 4096); // NOLINT

        BIO_ptr certOutputBio(BIO_new(BIO_s_mem()), BIO_free);
        int ret2 = PEM_write_bio_X509(certOutputBio.get(), cert.get());
        std::string certString = bioToString(certOutputBio, 32768); // NOLINT

        // result = (ret == 1) && (ret2 == 1); // OpenSSL return codes

        return TlsCertificate{.privateKey = keyString, .cert = certString};
    }

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
};

} // namespace streamr::dht::utils

#endif