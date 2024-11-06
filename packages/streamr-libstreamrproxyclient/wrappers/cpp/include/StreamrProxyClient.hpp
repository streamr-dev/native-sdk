#ifndef STREAMR_PROXY_CLIENT_HPP
#define STREAMR_PROXY_CLIENT_HPP

#include <string>
#include <vector>
#include <streamrproxyclient.h>
#include <variant>
struct ProxyAddress {
    std::string websocketUrl;
    std::string ethereumAddress;
};

struct ErrorInvalidEthereumAddress {
    static constexpr std::string_view name = ERROR_INVALID_ETHEREUM_ADDRESS;
};

struct ErrorInvalidStreamPartId {
    static constexpr std::string_view name = ERROR_INVALID_STREAM_PART_ID;
};

struct ErrorProxyClientNotFound {
    static constexpr std::string_view name = ERROR_PROXY_CLIENT_NOT_FOUND;
};

struct ErrorInvalidProxyUrl {
    static constexpr std::string_view name = ERROR_INVALID_PROXY_URL;
};

struct ErrorNoProxiesDefined {
    static constexpr std::string_view name = ERROR_NO_PROXIES_DEFINED;
};

struct ErrorProxyConnectionFailed {
    static constexpr std::string_view name = ERROR_PROXY_CONNECTION_FAILED;
};

struct ErrorProxyBroadcastFailed {
    static constexpr std::string_view name = ERROR_PROXY_BROADCAST_FAILED;
};

using ProxyErrorCode = std::variant<
    ErrorInvalidEthereumAddress,
    ErrorInvalidStreamPartId,
    ErrorProxyClientNotFound,
    ErrorInvalidProxyUrl,
    ErrorNoProxiesDefined,
    ErrorProxyConnectionFailed,
    ErrorProxyBroadcastFailed>;

struct ProxyError {
    ProxyAddress proxy;
    std::string message;
    ProxyErrorCode code;
};

/*
struct ProxyResult {
    std::vector<ProxyAddress> successful;
    std::vector<ProxyError> failed;
};

class StreamrProxyClient {
    StreamrProxyClient() {
        // call proxyClientNew
    }
    ~StreamrProxyClient() {
        // call proxyClientDelete
    }

    ProxyResult connect(const std::vector<ProxyAddress> proxies) {
            // call proxyClientConnect
        }
    
    ProxyResult publish(const std::string message) {
            // call proxyClientPublish
        }

};
*/

#endif // STREAMR_PROXY_CLIENT_HPP
