#ifndef StreamrProxyClient_hpp
#define StreamrProxyClient_hpp

#include <string>
#include <vector>
#include "streamrproxyclient.h"

namespace streamr::libstreamrproxyclient {

enum class StreamrProxyErrorCode {
    INVALID_ETHEREUM_ADDRESS,
    INVALID_STREAM_PART_ID,
    INVALID_PROXY_URL,
    NO_PROXIES_DEFINED,
    PROXY_CONNECTION_FAILED,
    UNKNOWN_ERROR // Always good to have an unknown error state
};

class Err : public std::runtime_error {
public:
    StreamrProxyErrorCode code; // NOLINT
    std::string message; // NOLINT

    Err(StreamrProxyErrorCode code, const std::string& message)
        : std::runtime_error(message), code(code), message(message) {}
};

struct InvalidEthereumAddress : public Err {
    explicit InvalidEthereumAddress(const std::string& message)
        : Err(StreamrProxyErrorCode::INVALID_ETHEREUM_ADDRESS, message) {}
};

struct InvalidStreamPartId : public Err {
    explicit InvalidStreamPartId(const std::string& message)
        : Err(StreamrProxyErrorCode::INVALID_STREAM_PART_ID, message) {}
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
    static const std::unordered_map<std::string, StreamrProxyErrorCode>
        errorMap = {
            {ERROR_INVALID_ETHEREUM_ADDRESS,
             StreamrProxyErrorCode::INVALID_ETHEREUM_ADDRESS},
            {ERROR_INVALID_STREAM_PART_ID,
             StreamrProxyErrorCode::INVALID_STREAM_PART_ID},
            {ERROR_INVALID_PROXY_URL, StreamrProxyErrorCode::INVALID_PROXY_URL},
            {ERROR_NO_PROXIES_DEFINED,
             StreamrProxyErrorCode::NO_PROXIES_DEFINED},
            {ERROR_PROXY_CONNECTION_FAILED,
             StreamrProxyErrorCode::PROXY_CONNECTION_FAILED}};

    if (cErrorCode == nullptr) {
        return StreamrProxyErrorCode::UNKNOWN_ERROR;
    }

    auto it = errorMap.find(cErrorCode);
    return it != errorMap.end() ? it->second
                                : StreamrProxyErrorCode::UNKNOWN_ERROR;
}

// Convert from C Proxy to C++ StreamrProxyAddress
inline StreamrProxyAddress convertProxyAddress(const Proxy& cProxy) {
    return StreamrProxyAddress{
        std::string(cProxy.websocketUrl), std::string(cProxy.ethereumAddress)};
}

inline StreamrProxyError convertError(const Error& cError) {
    return StreamrProxyError{
        std::string(cError.message),
        convertErrorCode(cError.code),
        convertProxyAddress(*cError.proxy)};
}

class StreamrProxyClient {
private:
    uint64_t clientHandle;

    // Helper method to convert C ProxyResult to C++ StreamrProxyResult
    StreamrProxyResult convertToStreamrResult(
        const ProxyResult* proxyResult, uint64_t numSuccess) {
        StreamrProxyResult streamrResult;
        streamrResult.numConnected = numSuccess;

        if (proxyResult != nullptr) {
            // Handle successful operations
            for (size_t i = 0; i < proxyResult->numSuccessful; i++) {
                streamrResult.successful.push_back(
                    convertProxyAddress(proxyResult->successful[i]));
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

public:

    StreamrProxyClient(
        std::string ownEthereumAddress, std::string streamPartId) {
        const ProxyResult* proxyResult = nullptr;
        this->clientHandle = proxyClientNew(
            &proxyResult, ownEthereumAddress.c_str(), streamPartId.c_str());

        if (proxyResult != nullptr && proxyResult->numErrors > 0) {
            std::string errorMessage = proxyResult->errors[0].message;
            StreamrProxyErrorCode errorCode =
                convertErrorCode(proxyResult->errors[0].code);
            proxyClientResultDelete(proxyResult);

            // Throw appropriate exception based on error code
            switch (errorCode) {
                case StreamrProxyErrorCode::INVALID_ETHEREUM_ADDRESS:
                    throw InvalidEthereumAddress(errorMessage);
                case StreamrProxyErrorCode::INVALID_STREAM_PART_ID:
                    throw InvalidStreamPartId(errorMessage);
                default:
                    throw Err(errorCode, errorMessage);
            }
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
                StreamrProxyAddress{} // Empty proxy address
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

        return convertToStreamrResult(proxyResult, numConnected);
    }

    StreamrProxyResult publish(
        const std::string& content, const std::string& ethereumPrivateKey) {
        const ProxyResult* proxyResult = nullptr;

        uint64_t numPublished = proxyClientPublish(
            &proxyResult,
            this->clientHandle,
            content.c_str(),
            content.length(),
            ethereumPrivateKey.c_str());

        return convertToStreamrResult(proxyResult, numPublished);
    }
};

} // namespace streamr::libstreamrproxyclient

#endif /* StreamrProxyClient_hpp */