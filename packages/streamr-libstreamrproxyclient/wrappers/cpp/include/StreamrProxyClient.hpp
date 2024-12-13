#ifndef STREAMR_PROXY_CLIENT_HPP
#define STREAMR_PROXY_CLIENT_HPP

#include <streamrproxyclient.h>
#include <cstring>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace streamrproxyclient {

/** 
 * @brief Error indicating an invalid Ethereum address
 */
struct ErrorInvalidEthereumAddress {
    static constexpr const char* name = ERROR_INVALID_ETHEREUM_ADDRESS;
};

/** 
 * @brief Error indicating an invalid stream part ID
 */
struct ErrorInvalidStreamPartId {
    static constexpr const char* name = ERROR_INVALID_STREAM_PART_ID;
};

/** 
 * @brief Error indicating proxy client was not found
 */
struct ErrorProxyClientNotFound {
    static constexpr const char* name = ERROR_PROXY_CLIENT_NOT_FOUND;
};

/** 
 * @brief Error indicating an invalid proxy URL
 */
struct ErrorInvalidProxyUrl {
    static constexpr const char* name = ERROR_INVALID_PROXY_URL;
};

/** 
 * @brief Error indicating no proxies were defined
 */
struct ErrorNoProxiesDefined {
    static constexpr const char* name = ERROR_NO_PROXIES_DEFINED;
};

/** 
 * @brief Error indicating proxy connection failed
 */
struct ErrorProxyConnectionFailed {
    static constexpr const char* name = ERROR_PROXY_CONNECTION_FAILED;
};

/** 
 * @brief Error indicating proxy broadcast failed
 */
struct ErrorProxyBroadcastFailed {
    static constexpr const char* name = ERROR_PROXY_BROADCAST_FAILED;
};

/** 
 * @brief Variant type containing all possible proxy error codes
 */
using StreamrProxyErrorCode = std::variant<
    ErrorInvalidEthereumAddress,
    ErrorInvalidStreamPartId,
    ErrorProxyClientNotFound,
    ErrorInvalidProxyUrl,
    ErrorNoProxiesDefined,
    ErrorProxyConnectionFailed,
    ErrorProxyBroadcastFailed>;

/** 
 * @brief Represents a Streamr proxy address
 */
struct StreamrProxyAddress {
    std::string websocketUrl;     /**< WebSocket URL of the proxy */
    std::string ethereumAddress;  /**< Ethereum address of the proxy */

    static StreamrProxyAddress fromCProxy(const Proxy* proxy) {
        return StreamrProxyAddress{
            std::string(proxy->websocketUrl),
            std::string(proxy->ethereumAddress)};
    }
};

/** 
 * @brief Custom exception class for Streamr proxy errors
 */
struct StreamrProxyError : public std::runtime_error {
    StreamrProxyAddress proxy;    /**< The proxy where the error occurred */
    StreamrProxyErrorCode code;   /**< The error code */

    // NOLINTNEXTLINE(google-explicit-constructor)
    StreamrProxyError(const Error* error)
        : std::runtime_error(error->message),
          code(getErrorCode(error->code)) {
        if (error->proxy) {
            proxy = StreamrProxyAddress::fromCProxy(error->proxy);
        }
    }

private:

    static StreamrProxyErrorCode getErrorCode(const char* code) {
        if (strcmp(code, ERROR_INVALID_ETHEREUM_ADDRESS) == 0) {
            return ErrorInvalidEthereumAddress{};
        }
        if (strcmp(code, ERROR_INVALID_STREAM_PART_ID) == 0) {
            return ErrorInvalidStreamPartId{};
        }
        if (std::strcmp(code, ERROR_PROXY_CLIENT_NOT_FOUND) == 0) {
            return ErrorProxyClientNotFound{};
        }
        if (std::strcmp(code, ERROR_INVALID_PROXY_URL) == 0) {
            return ErrorInvalidProxyUrl{};
        }
        if (std::strcmp(code, ERROR_NO_PROXIES_DEFINED) == 0) {
            return ErrorNoProxiesDefined{};
        }
        if (std::strcmp(code, ERROR_PROXY_CONNECTION_FAILED) == 0) {
            return ErrorProxyConnectionFailed{};
        }
        if (std::strcmp(code, ERROR_PROXY_BROADCAST_FAILED) == 0) {
            return ErrorProxyBroadcastFailed{};
        }
        return ErrorInvalidProxyUrl{};
    }
};

/** 
 * @brief Contains results of proxy operations
 */
struct StreamrProxyResult {
    std::vector<StreamrProxyAddress> successful;  /**< List of successful proxy operations */
    std::vector<StreamrProxyError> errors;        /**< List of failed proxy operations */

    // NOLINTNEXTLINE(google-explicit-constructor)
    StreamrProxyResult(const ProxyResult* result) {
        this->successful.reserve(result->numSuccessful);
        for (size_t i = 0; i < result->numSuccessful; i++) {
            this->successful.push_back(
                StreamrProxyAddress::fromCProxy(&result->successful[i]));
        }
        this->errors.reserve(result->numErrors);
        for (size_t i = 0; i < result->numErrors; i++) {
            this->errors.emplace_back(&result->errors[i]);
        }
    }
};

class SharedLibraryWrapper {
public:
    SharedLibraryWrapper() { proxyClientInitLibrary(); }
    ~SharedLibraryWrapper() { proxyClientCleanupLibrary(); }
};

/** 
 * @brief Main client class for interacting with Streamr proxies
 */
class StreamrProxyClient {
private:
    static SharedLibraryWrapper sharedLibraryWrapper;
    uint64_t proxyClientHandle = 0;
    std::string ethereumPrivateKey;

public:
    /** 
     * @brief Constructs a new StreamrProxyClient
     * @param ownEthereumAddress Ethereum address of the client
     * @param ethereumPrivateKey Private key for the Ethereum address
     * @param streamPartId Stream part ID
     * @throw StreamrProxyError if client creation fails
     */

    StreamrProxyClient(
        const std::string& ownEthereumAddress, // NOLINT
        const std::string& ethereumPrivateKey, // NOLINT
        const std::string& streamPartId)
        : ethereumPrivateKey(ethereumPrivateKey) {
        const ProxyResult* result;
        this->proxyClientHandle = proxyClientNew(
            &result, ownEthereumAddress.c_str(), streamPartId.c_str());
        if (result->numErrors > 0) {
            auto error = StreamrProxyError(&result->errors[0]);
            proxyClientResultDelete(result);
            throw error; // NOLINT
        }
        proxyClientResultDelete(result);
    }

    ~StreamrProxyClient() {
        if (this->proxyClientHandle != 0) {
            const ProxyResult* result;
            proxyClientDelete(&result, this->proxyClientHandle);
            proxyClientResultDelete(result);
        }
    }

    /** 
     * @brief Connects to specified proxies
     * @param proxies Vector of proxy addresses to connect to
     * @return Result containing successful connections and errors
     */
    [[nodiscard]] StreamrProxyResult connect(
        const std::vector<StreamrProxyAddress>& proxies) const {
        const ProxyResult* result;
        std::vector<Proxy> cProxies;
        cProxies.reserve(proxies.size());
        for (const auto& proxy : proxies) {
            cProxies.push_back(
                {proxy.websocketUrl.c_str(), proxy.ethereumAddress.c_str()});
        }
        proxyClientConnect(
            &result, this->proxyClientHandle, cProxies.data(), cProxies.size());
        StreamrProxyResult streamrProxyResult(result);
        proxyClientResultDelete(result);
        return streamrProxyResult;
    }

    /** 
     * @brief Publishes a message through connected proxies
     * @param message Message to publish
     * @return Result containing successful publications and errors
     */
    [[nodiscard]] StreamrProxyResult publish(const std::string& message) const {
        const ProxyResult* result;
        proxyClientPublish(
            &result,
            this->proxyClientHandle,
            message.data(),
            message.size(),
            this->ethereumPrivateKey.c_str());
        StreamrProxyResult streamrProxyResult(result);
        proxyClientResultDelete(result);
        return streamrProxyResult;
    }
};

} // namespace streamrproxyclient
#endif // STREAMR_PROXY_CLIENT_HPP
