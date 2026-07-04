#include <gtest/gtest.h>

import streamr.dht.CertificateHelper;
import streamr.logger;

using streamr::dht::helpers::CertificateHelper;
using streamr::logger::SLogger;

TEST(CertificateHelperTest, createSelfSignedCertificate) {
    auto certificate =
        CertificateHelper::createSelfSignedCertificate(1000); // NOLINT
    SLogger::info("Certificate: {}", certificate);
    EXPECT_FALSE(certificate.privateKey.empty());
    EXPECT_FALSE(certificate.cert.empty());
}