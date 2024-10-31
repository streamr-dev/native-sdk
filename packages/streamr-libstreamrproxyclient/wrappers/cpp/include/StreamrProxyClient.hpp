#ifndef STREAMR_PROXY_CLIENT_HPP
#define STREAMR_PROXY_CLIENT_HPP

#include <string>
#include <vector>

struct ProxyAddress {
    std::string websocketUrl;
    std::string ethereumAddress;
};

enum class ProxyErrorCode {
    // defined in streamrproxyclient.h
};

struct ProxyError {
    ProxyAddress proxy;
    std::string message;
    ProxyErrorCode code;
};

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

#endif // STREAMR_PROXY_CLIENT_HPP
