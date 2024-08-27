#include <gtest/gtest.h>
#include "streamr-dht/utils/CertificateHelper.hpp"
#include "streamr-logger/SLogger.hpp"

using streamr::logger::SLogger;
using streamr::dht::utils::CertificateHelper;

TEST(CertificateHelperTest, createSelfSignedCertificate) {
    auto certificate = CertificateHelper::createSelfSignedCertificate(1000); // NOLINT
    SLogger::info("Certificate: {}", certificate);
    EXPECT_FALSE(certificate.privateKey.empty());
    EXPECT_FALSE(certificate.cert.empty());
}