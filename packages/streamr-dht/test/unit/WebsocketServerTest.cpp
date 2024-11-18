#include "streamr-dht/connection/websocket/WebsocketServer.hpp"
#include <gtest/gtest.h>
#include "streamr-dht/helpers/Errors.hpp"

using streamr::dht::connection::websocket::TlsCertificateFiles;
using streamr::dht::connection::websocket::WebsocketServer;
using streamr::dht::connection::websocket::WebsocketServerConfig;
using streamr::dht::helpers::WebsocketServerStartError;

TEST(WebsocketServerTest, TestStartAndStopServer) {
    WebsocketServerConfig config{
        .portRange = {10000, 10001}, // NOLINT
        .enableTls = false,
        .tlsCertificateFiles = std::nullopt,
        .maxMessageSize = std::nullopt};

    WebsocketServer server(std::move(config));
    server.start();
    server.stop();
}

TEST(WebsocketServerTest, TestServerThrowsIfPortIsInUse) {
    WebsocketServerConfig config{
        .portRange = {10001, 10001}, // NOLINT
        .enableTls = false,
        .tlsCertificateFiles = std::nullopt,
        .maxMessageSize = std::nullopt};

    WebsocketServerConfig config2 = config;

    WebsocketServer server(std::move(config));
    server.start();

    WebsocketServer server2(std::move(config2));
    EXPECT_THROW(server2.start(), WebsocketServerStartError); // NOLINT

    server.stop();
}

TEST(WebsocketServerTest, TestCanThrowIfCertificateNotFound) {
    WebsocketServerConfig config{
        .portRange = {10000, 10001}, // NOLINT
        .enableTls = false,
        .tlsCertificateFiles =
            std::optional<TlsCertificateFiles>{TlsCertificateFiles{"", ""}},
        .maxMessageSize = std::nullopt};
    WebsocketServer server(std::move(config));
    EXPECT_THROW(server.start(), WebsocketServerStartError); // NOLINT
    server.stop();
}

TEST(WebsocketServerTest, TestStartAndStopServerWithSpecifiedCertificate) {
    WebsocketServerConfig config{
        .portRange = {10000, 10001}, // NOLINT
        .enableTls = false,
        .tlsCertificateFiles =
            std::optional<TlsCertificateFiles>{TlsCertificateFiles{
                "../test/unit/example.key", "../test/unit/example.crt"}},
        .maxMessageSize = std::nullopt};
    WebsocketServer server(std::move(config));
    server.start();
    server.stop();
}

TEST(WebsocketServerTest, UpdateCertificate) {
    WebsocketServerConfig config{
        .portRange = {10004, 10005}, // NOLINT
        .enableTls = true,
        .tlsCertificateFiles =
            std::optional<TlsCertificateFiles>{TlsCertificateFiles{
                "../test/unit/example.key", "../test/unit/example.crt"}},
        .maxMessageSize = std::nullopt};

    WebsocketServer server(std::move(config));
    server.start();
    std::string certContent = R"(-----BEGIN CERTIFICATE-----
MIIDNzCCAh+gAwIBAgIUE1THW7vt62i4POz+Ixv5GI4RPFAwDQYJKoZIhvcNAQEL
BQAwRDELMAkGA1UEBhMCRkkxEDAOBgNVBAgMB1V1c2ltYWExETAPBgNVBAcMCEhl
bHNpbmtpMRAwDgYDVQQKDAdTdHJlYW1yMB4XDTI0MDgyOTEyMDYzOFoXDTI1MDgy
OTEyMDYzOFowRDELMAkGA1UEBhMCRkkxEDAOBgNVBAgMB1V1c2ltYWExETAPBgNV
BAcMCEhlbHNpbmtpMRAwDgYDVQQKDAdTdHJlYW1yMIIBIjANBgkqhkiG9w0BAQEF
AAOCAQ8AMIIBCgKCAQEAuTEpicjIL6B85GCKw9GaRQxDWXoi8FW0YFKX69tmBd7p
RjvTvL7TlhTcv22kFP0Sqz8RQ8Pq5zBUUgaxaXfOdkPHyv4c4k1WgjOVyeE8gs89
HKLo5FgmXgqTBJkOFYjRIP+zoOzAVbdMXDD65An5W5bJ2Hm1XySR9AUqHvA1QlB+
QJ1lFAT+GcfuGQSy5dSJpJ05AGCaKrQqEj/hC/cmF8WRDph8/Q7upqXSGFBQtUYn
hhPd+1+5DUuSzxHqSzITKP4J2bl3IEpVDhL5zWV4tP0EXuOtFNbD7Ej1hNiFWj7b
Txn5a9DeBhO1injncSDhkXmD1Bll5RVR7eJ1kXYvJQIDAQABoyEwHzAdBgNVHQ4E
FgQU+H6mh7bjjyT/cEvdyJ+lcDLT6PMwDQYJKoZIhvcNAQELBQADggEBAJ2m8zDF
PRI0uJ/JvqD8Vtvk8Xu9gcYzrqM5+XUm5qFZ1TOaoxyXQxAINjLf4bGi7oeQxjQG
4VgkKpj/+W4QyRA5rRZjwf4a75Yhh/8Ni8oo560LteZ97OLjNn5juScXYpU5YczG
SyGUbKiN2+Ql2R/oTDUOkh6ILwQKYlrWMbwCn/t1hL2rhifwKQVR0y28QZK2pB+/
YXEJVXIA0JaAMmh/Mzz3xc0Wm+ZNhFg2tYSW5PyAPHe8puEOrtxKdX72/FQVYM9z
F+9OvISm466xjM/uaGEPndhA7O9WdeRA2qCKmuxFGYRKuaZbmZroIM1KOeDfiZRS
GEiOqUnpnId09QU=
-----END CERTIFICATE-----)";

    std::string keyContent = R"(-----BEGIN PRIVATE KEY-----
MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQC5MSmJyMgvoHzk
YIrD0ZpFDENZeiLwVbRgUpfr22YF3ulGO9O8vtOWFNy/baQU/RKrPxFDw+rnMFRS
BrFpd852Q8fK/hziTVaCM5XJ4TyCzz0coujkWCZeCpMEmQ4ViNEg/7Og7MBVt0xc
MPrkCflblsnYebVfJJH0BSoe8DVCUH5AnWUUBP4Zx+4ZBLLl1ImknTkAYJoqtCoS
P+EL9yYXxZEOmHz9Du6mpdIYUFC1RieGE937X7kNS5LPEepLMhMo/gnZuXcgSlUO
EvnNZXi0/QRe460U1sPsSPWE2IVaPttPGflr0N4GE7WKeOdxIOGReYPUGWXlFVHt
4nWRdi8lAgMBAAECggEACi/XHhoqUNxeIl3hJDZiawvqpIBgbaH5Qxcn/jRaX1ZM
tK1Tz10b3HSXU/xe+zyUi1DzzSt4v3DcIor1tVx+weimk9b4lcY1TQIzaiB2qKdn
sCyR7QWgTqn8PlOA/9Q/1LXcFq0PQ2fKzoNvOZ4G1m/r+HlxbaNgV/D0yTDjbRtS
PUpelR1ReoWzBR9t8ajbRtNNWPTANcKzJrUS0hVs1BDybFp6lY3Oy7P0lqSV8WG8
+Iex5PCA5MKyDCSzENbLPz5vo6Mw3yJhvPUqZPIY0c8fo3RofXCc32/AU9zaUCYU
mr846XoZn1S/EUFt1ZrlHIdYAlvE4LNb9DtPyt2RvQKBgQD4SXLYJ+kmQanZAXxj
f1L7O7CYrNZPe0uHCfHhxCueO5pkd2Cdfc6RJ/unMbN/zEdm/GS+6YIPlM2C9jj2
DQiWOd39wP7pe/Hd5hOOGKRT2XeoJKckKv1KccAIEC0yyVQHExbCw7+5x6IcYYRK
UyLj0kGqCce4Zy5GrVcDFGL/ewKBgQC+8fBmeF2O/yln57WKVZ6IrjD/v69rYri+
xPgI2yDk2fuz/Xuxw0DokmyCD8mVVT2k5RnSr7LIRXaws6eoFEZQH117OdUllel7
ZJQGXQToE9Z+6VmVNZzOwtouvl4MZ9gZaa+Vw3oXSf4ai2oiDNOLvehZfezk61Dg
iBs+68H53wKBgFLJSr3AMQFMi7GLyUnzvlt+v5doqdy+o6RXMIuyuUh9XzF4jIJ2
3FWSG1rYO521I2m3ZnAxs+g2GYA9USjZl69fhCGEJHr9lNwERyjuFnzO2hL6hbCN
lP8phnopyqhQcPAa8U/nrRno8qi76zxNFCkahkKIGEvoO4ndalHgjlHZAoGARLCU
Ysh6H677Hj3kuNcEKPdA+T/jwyXIgBgrgkQSGUGxopZVoSU0fHXwQvma8vWvL1qb
Z4d9MT6L7BU1AuoIQVqHLoUngFXloFYWShO8aCB60Tzw1RRsTJUcGCSzgJL60mmK
mL2xdh9QIgx1Kbqjf2nZ5BfA2LkuZxePdZsqNNECgYAOCvCLkhN2WqtajelKENXX
ppZ1tpzIEA0juivu93S/rDo8NqmH93iv1hbH7CSL/VfIjwkR1ciSL7Fm4nNTbtjt
HdxDqpEPa/uGsv+EPnapodKuXO5a2rUCRMEJBcqRIWNZE4Gsey0s61hGCz5O9PpG
noOPvyLe52Hc2twPb9+w8g==
-----END PRIVATE KEY-----)";
    EXPECT_NO_THROW(server.updateCertificate(certContent, keyContent));

    server.stop();
}

TEST(WebsocketServerTest, UpdateCertificateWithInvalidCertificate) {
    WebsocketServerConfig config{
        .portRange = {10004, 10005}, // NOLINT
        .enableTls = true,
        .tlsCertificateFiles =
            std::optional<TlsCertificateFiles>{TlsCertificateFiles{
                "../test/unit/example.key", "../test/unit/example.crt"}},
        .maxMessageSize = std::nullopt};

    WebsocketServer server(std::move(config));
    server.start();
    std::string certContent = R"(-----BEGIN CERTIFICATE-----
MIIDNzCCAh+gAwIBAgIUE1THW7vt62i4POz+Ixv5GI4RPFAwDQYJKoZIhvcNAQEL
BQAwRDELMAkGA1UEBhMCRkkxEDAOBgNVBAgMB1V1c2ltYWExETAPBgNVBAcMCEhl
bHNpbmtpMRAwDgYDVQQKDAdTdHJlYW1yMB4XDTI0MDgyOTEyMDYzOFoXDTI1MDgy
OTEyMDYzOFowRDELMAkGA1UEBhMCRkkxEDAOBgNVBAgMB1V1c2ltYWExETAPBgNV
BAcMCEhlbHNpbmtpMRAwDgYDVQQKDAdTdHJlYW1yMIIBIjANBgkqhkiG9w0BAQEF
)";

    std::string keyContent = R"(-----BEGIN PRIVATE KEY-----
MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQC5MSmJyMgvoHzk
YIrD0ZpFDENZeiLwVbRgUpfr22YF3ulGO9O8vtOWFNy/baQU/RKrPxFDw+rnMFRS
BrFpd852Q8fK/hziTVaCM5XJ4TyCzz0coujkWCZeCpMEmQ4ViNEg/7Og7MBVt0xc
MPrkCflblsnYebVfJJH0BSoe8DVCUH5AnWUUBP4Zx+4ZBLLl1ImknTkAYJoqtCoS
P+EL9yYXxZEOmHz9Du6mpdIYUFC1RieGE937X7kNS5LPEepLMhMo/gnZuXcgSlUO
EvnNZXi0/QRe460U1sPsSPWE2IVaPttPGflr0N4GE7WKeOdxIOGReYPUGWXlFVHt
4nWRdi8lAgMBAAECggEACi/XHhoqUNxeIl3hJDZiawvqpIBgbaH5Qxcn/jRaX1ZM
tK1Tz10b3HSXU/xe+zyUi1DzzSt4v3DcIor1tVx+weimk9b4lcY1TQIzaiB2qKdn
sCyR7QWgTqn8PlOA/9Q/1LXcFq0PQ2fKzoNvOZ4G1m/r+HlxbaNgV/D0yTDjbRtS
PUpelR1ReoWzBR9t8ajbRtNNWPTANcKzJrUS0hVs1BDybFp6lY3Oy7P0lqSV8WG8
+Iex5PCA5MKyDCSzENbLPz5vo6Mw3yJhvPUqZPIY0c8fo3RofXCc32/AU9zaUCYU
mr846XoZn1S/EUFt1ZrlHIdYAlvE4LNb9DtPyt2RvQKBgQD4SXLYJ+kmQanZAXxj
f1L7O7CYrNZPe0uHCfHhxCueO5pkd2Cdfc6RJ/unMbN/zEdm/GS+6YIPlM2C9jj2
DQiWOd39wP7pe/Hd5hOOGKRT2XeoJKckKv1KccAIEC0yyVQHExbCw7+5x6IcYYRK
UyLj0kGqCce4Zy5GrVcDFGL/ewKBgQC+8fBmeF2O/yln57WKVZ6IrjD/v69rYri+
xPgI2yDk2fuz/Xuxw0DokmyCD8mVVT2k5RnSr7LIRXaws6eoFEZQH117OdUllel7
ZJQGXQToE9Z+6VmVNZzOwtouvl4MZ9gZaa+Vw3oXSf4ai2oiDNOLvehZfezk61Dg
iBs+68H53wKBgFLJSr3AMQFMi7GLyUnzvlt+v5doqdy+o6RXMIuyuUh9XzF4jIJ2
3FWSG1rYO521I2m3ZnAxs+g2GYA9USjZl69fhCGEJHr9lNwERyjuFnzO2hL6hbCN
lP8phnopyqhQcPAa8U/nrRno8qi76zxNFCkahkKIGEvoO4ndalHgjlHZAoGARLCU
Ysh6H677Hj3kuNcEKPdA+T/jwyXIgBgrgkQSGUGxopZVoSU0fHXwQvma8vWvL1qb
Z4d9MT6L7BU1AuoIQVqHLoUngFXloFYWShO8aCB60Tzw1RRsTJUcGCSzgJL60mmK
mL2xdh9QIgx1Kbqjf2nZ5BfA2LkuZxePdZsqNNECgYAOCvCLkhN2WqtajelKENXX
ppZ1tpzIEA0juivu93S/rDo8NqmH93iv1hbH7CSL/VfIjwkR1ciSL7Fm4nNTbtjt
HdxDqpEPa/uGsv+EPnapodKuXO5a2rUCRMEJBcqRIWNZE4Gsey0s61hGCz5O9PpG
noOPvyLe52Hc2twPb9+w8g==
-----END PRIVATE KEY-----)";

    EXPECT_THROW( // NOLINT
        server.updateCertificate(certContent, keyContent), // NOLINT
        WebsocketServerStartError); // NOLINT

    server.stop();
}
