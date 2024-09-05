#include <gtest/gtest.h>
#include "streamr-dht/helpers/CertificateHelper.hpp"
#include "streamr-logger/SLogger.hpp"

using streamr::logger::SLogger;
using streamr::dht::helpers::CertificateHelper;

TEST(CertificateHelperTest, createSelfSignedCertificate) {
    auto certificate = CertificateHelper::createSelfSignedCertificate(1000); // NOLINT
    SLogger::info("Certificate: {}", certificate);
    EXPECT_FALSE(certificate.privateKey.empty());
    EXPECT_FALSE(certificate.cert.empty());
}