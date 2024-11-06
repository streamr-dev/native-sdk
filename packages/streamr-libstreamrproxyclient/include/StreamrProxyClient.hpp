#ifndef StreamrProxyClient_hpp
#define StreamrProxyClient_hpp

#include <string>
#include<vector>
#include "streamrproxyclient.h"

namespace streamr::libstreamrproxyclient {

enum class StreamrProxyErrorCode {
    INVALID_ETHEREUM_ADDRESS,
    INVALID_STREAM_PART_ID,
    INVALID_PROXY_URL,
    PROXY_CLIENT_NOT_FOUND,
    NO_PROXIES_DEFINED,
    PROXY_CONNECTION_FAILED,
    UNKNOWN_ERROR  // Always good to have an unknown error state
};

struct StreamrProxyAddress {
    std::string websocketUrl;
    std::string ethereumAddress;
};

struct StreamrProxyError {
    std::string message;
    StreamrProxyErrorCode code;
    StreamrProxyAddress proxy;
};

struct StreamrProxyResult {
    uint64_t numConnected; // Number of successfully connected proxies
    std::vector<StreamrProxyAddress> successful;
    std::vector<StreamrProxyError> failed;
};

// Helper function to convert C error codes (strings) to C++ enum
inline StreamrProxyErrorCode convertErrorCode(const char* cErrorCode) {
    static const std::unordered_map<std::string, StreamrProxyErrorCode> errorMap = {
        {ERROR_INVALID_ETHEREUM_ADDRESS, StreamrProxyErrorCode::INVALID_ETHEREUM_ADDRESS},
        {ERROR_INVALID_STREAM_PART_ID, StreamrProxyErrorCode::INVALID_STREAM_PART_ID},
        {ERROR_INVALID_PROXY_URL, StreamrProxyErrorCode::INVALID_PROXY_URL},
        {ERROR_PROXY_CLIENT_NOT_FOUND, StreamrProxyErrorCode::PROXY_CLIENT_NOT_FOUND},
        {ERROR_NO_PROXIES_DEFINED, StreamrProxyErrorCode::NO_PROXIES_DEFINED},
        {ERROR_PROXY_CONNECTION_FAILED, StreamrProxyErrorCode::PROXY_CONNECTION_FAILED}
    };

    if (cErrorCode == nullptr) {
        return StreamrProxyErrorCode::UNKNOWN_ERROR;
    }

    auto it = errorMap.find(cErrorCode);
    return it != errorMap.end() ? it->second : StreamrProxyErrorCode::UNKNOWN_ERROR;
}

// Convert from C Proxy to C++ StreamrProxyAddress
inline StreamrProxyAddress convertProxyAddress(const Proxy& cProxy) {
    return StreamrProxyAddress{
        std::string(cProxy.websocketUrl),
        std::string(cProxy.ethereumAddress)
    };
}

inline StreamrProxyError convertError(const Error& cError) {
    return StreamrProxyError{
        std::string(cError.message),
        convertErrorCode(cError.code),
        convertProxyAddress(*cError.proxy)
    };
}

class StreamrProxyClient {
private:
    uint64_t clientHandle;

public:
    StreamrProxyClient(
        std::string ownEthereumAddress, std::string streamPartId) {
        const ProxyResult* proxyResult = nullptr;
        this->clientHandle = proxyClientNew(
            &proxyResult, ownEthereumAddress.c_str(), streamPartId.c_str());

        if (proxyResult != nullptr && proxyResult->numErrors > 0) {
            std::string errorMessage =
                "Failed to initialize StreamrProxyClient:\n";
            for (size_t i = 0; i < proxyResult->numErrors; i++) {
                errorMessage.append("- ")
                    .append(proxyResult->errors[i].message)
                    .append(" (Error code: ")
                    .append(proxyResult->errors[i].code)
                    .append(")\n");
            }
            throw std::runtime_error(errorMessage);
        }
        proxyClientResultDelete(proxyResult);
    }

    ~StreamrProxyClient() noexcept {
        const ProxyResult* proxyResult = nullptr;
        proxyClientDelete(&proxyResult, this->clientHandle);
        proxyClientResultDelete(proxyResult);
    }

    StreamrProxyResult connect(
        const std::vector<StreamrProxyAddress>& proxies) {
        const ProxyResult* proxyResult = nullptr;
        StreamrProxyResult streamrResult;
        
        // Handle empty proxy list case first
        if (proxies.empty()) {
            streamrResult.numConnected = 0;
            streamrResult.failed.push_back(StreamrProxyError{
                "No proxies defined",
                StreamrProxyErrorCode::NO_PROXIES_DEFINED,
                StreamrProxyAddress{}  // Empty proxy address
            });
            return streamrResult;
        }
        // Convert C++ proxy addresses to C structure
        std::vector<Proxy> cProxies;
        cProxies.reserve(proxies.size());

        for (const auto& proxy : proxies) {
            cProxies.push_back(Proxy{
                proxy.websocketUrl.c_str(), proxy.ethereumAddress.c_str()});
        }

        uint64_t numConnected = proxyClientConnect(
            &proxyResult, this->clientHandle, cProxies.data(), cProxies.size());

        // Convert C result to C++ result
        streamrResult.numConnected =
            numConnected; // Store the number of connected proxies
        if (proxyResult != nullptr) {
            // Handle successful connections
            for (size_t i = 0; i < proxyResult->numSuccessful; i++) {
                streamrResult.successful.push_back(convertProxyAddress(proxyResult->successful[i]));
            }

            // Handle errors
            for (size_t i = 0; i < proxyResult->numErrors; i++) {
                streamrResult.failed.push_back(
                    convertError(proxyResult->errors[i]));
            }
            proxyClientResultDelete(proxyResult);
        }

        return streamrResult;
    }


};

} // namespace streamr::libstreamrproxyclient

#endif /* StreamrProxyClient_hpp */
