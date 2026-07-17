// Implementation of the public C API (streamrproxyclient.h — the proxy
// client — and streamrnode.h — the full network node; the two APIs
// share the result registry and the library lifecycle, so they live in
// one translation unit).
//
// CONSOLIDATED (MODERNIZATION.md Phase 2.6 consolidation): the former
// internal header LibProxyClientApi.hpp was merged into this file and
// the sibling streamr packages are consumed as C++ modules (import)
// instead of textual includes. Only third-party libraries, the standard
// library and the public C API headers remain textual includes.

// Coroutine definitions need std::coroutine_traits declared in THIS
// translation unit; it cannot arrive through an imported BMI.
#include <coroutine> // IWYU pragma: keep

#include <ada.h>
#include <stdint.h> // NOLINT
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <folly/Singleton.h>
#include "streamrnode.h"
#include "streamrproxyclient.h"

import streamr.trackerlessnetwork.protos;
import streamr.dht.ConnectionManager;
import streamr.dht.Connectivity;
import streamr.dht.ConnectorFacade;
import streamr.dht.DhtNode;
import streamr.dht.FakeTransport;
import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.eventemitter.EventEmitter;
import streamr.logger.SLogger;
import streamr.trackerlessnetwork.ContentDeliveryManager;
import streamr.trackerlessnetwork.NetworkNode;
import streamr.trackerlessnetwork.NetworkStack;
import streamr.trackerlessnetwork.ProxyClient;
import streamr.utils.BinaryUtils;
import streamr.utils.CoroutineHelper;
import streamr.utils.EthereumAddress;
import streamr.utils.SigningUtils;
import streamr.utils.StreamPartID;

namespace streamr::libstreamrproxyclient {

using ::dht::ConnectivityMethod;
using ::dht::ConnectivityResponse;
using ::dht::NodeType;
using ::dht::PeerDescriptor;
using streamr::dht::DhtNodeOptions;
using streamr::dht::Identifiers;
using streamr::dht::connection::ConnectionManager;
using streamr::dht::connection::ConnectionManagerOptions;
using streamr::dht::connection::DefaultConnectorFacade;
using streamr::dht::connection::DefaultConnectorFacadeOptions;
using streamr::dht::helpers::Connectivity;
using streamr::dht::transport::FakeEnvironment;
using streamr::dht::transport::FakeTransport;
using streamr::eventemitter::HandlerToken;
using streamr::logger::SLogger;
using streamr::trackerlessnetwork::ContentDeliveryManagerOptions;
using streamr::trackerlessnetwork::createNetworkNode;
using streamr::trackerlessnetwork::NetworkNode;
using streamr::trackerlessnetwork::NetworkOptions;
using streamr::trackerlessnetwork::proxy::ProxyClient;
using streamr::trackerlessnetwork::proxy::ProxyClientOptions;
using streamr::utils::BinaryUtils;
using streamr::utils::blockingWait;
using streamr::utils::EthereumAddress;
using streamr::utils::SigningUtils;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::toEthereumAddress;

class ProxyCpp {
private:
    std::string ethereumAddressString;
    std::string websocketUrlString;
    Proxy proxy;

public:
    ProxyCpp(
        const std::string& ethereumAddress, const std::string& websocketUrl) {
        this->ethereumAddressString = ethereumAddress;
        this->websocketUrlString = websocketUrl;
        this->proxy.ethereumAddress = this->ethereumAddressString.c_str();
        this->proxy.websocketUrl = this->websocketUrlString.c_str();
    }
    ProxyCpp(const ProxyCpp& other) {
        this->ethereumAddressString = other.ethereumAddressString;
        this->websocketUrlString = other.websocketUrlString;
        this->proxy.ethereumAddress = this->ethereumAddressString.c_str();
        this->proxy.websocketUrl = this->websocketUrlString.c_str();
    }

    ProxyCpp(ProxyCpp&& other) noexcept {
        this->ethereumAddressString = std::move(other.ethereumAddressString);
        this->websocketUrlString = std::move(other.websocketUrlString);
        this->proxy.ethereumAddress = this->ethereumAddressString.c_str();
        this->proxy.websocketUrl = this->websocketUrlString.c_str();
    }

    ProxyCpp& operator=(const ProxyCpp& other) {
        if (this == &other) {
            return *this;
        }
        this->ethereumAddressString = other.ethereumAddressString;
        this->websocketUrlString = other.websocketUrlString;
        this->proxy.ethereumAddress = this->ethereumAddressString.c_str();
        this->proxy.websocketUrl = this->websocketUrlString.c_str();
        return *this;
    }

    ProxyCpp& operator=(ProxyCpp&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        this->ethereumAddressString = std::move(other.ethereumAddressString);
        this->websocketUrlString = std::move(other.websocketUrlString);
        this->proxy.ethereumAddress = this->ethereumAddressString.c_str();
        this->proxy.websocketUrl = this->websocketUrlString.c_str();
        return *this;
    }
    [[nodiscard]] const Proxy* getProxy() const { return &this->proxy; }
};

class ErrorCpp {
private:
    std::string messageString;
    std::string codeString;
    std::optional<ProxyCpp> proxyCpp;

    Error error;

public:
    ErrorCpp(
        const std::string& message, // NOLINT
        const std::string& code,
        const std::optional<ProxyCpp>& proxyCpp) {
        this->proxyCpp = proxyCpp;

        this->messageString = std::string(message);
        this->codeString = std::string(code);
        this->error.message = this->messageString.c_str();
        this->error.code = this->codeString.c_str();
        this->error.proxy = this->proxyCpp.has_value()
            ? this->proxyCpp.value().getProxy()
            : nullptr;
    }
    ErrorCpp(const ErrorCpp& other) {
        this->messageString = other.messageString;
        this->codeString = other.codeString;
        this->proxyCpp = other.proxyCpp;
        this->error.message = this->messageString.c_str();
        this->error.code = this->codeString.c_str();
        this->error.proxy =
            this->proxyCpp ? this->proxyCpp->getProxy() : nullptr;
    }
    ErrorCpp(ErrorCpp&& other) noexcept {
        this->messageString = std::move(other.messageString);
        this->codeString = std::move(other.codeString);
        this->proxyCpp = std::move(other.proxyCpp);
        this->error.message = this->messageString.c_str();
        this->error.code = this->codeString.c_str();
        this->error.proxy =
            this->proxyCpp ? this->proxyCpp->getProxy() : nullptr;
    }
    ErrorCpp& operator=(const ErrorCpp& other) {
        if (this == &other) {
            return *this;
        }
        this->messageString = other.messageString;
        this->codeString = other.codeString;
        this->proxyCpp = other.proxyCpp;
        this->error.message = this->messageString.c_str();
        this->error.code = this->codeString.c_str();
        this->error.proxy =
            this->proxyCpp ? this->proxyCpp->getProxy() : nullptr;
        return *this;
    }

    ErrorCpp& operator=(ErrorCpp&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        this->messageString = std::move(other.messageString);
        this->codeString = std::move(other.codeString);
        this->proxyCpp = std::move(other.proxyCpp);
        this->error.message = this->messageString.c_str();
        this->error.code = this->codeString.c_str();
        this->error.proxy =
            this->proxyCpp ? this->proxyCpp->getProxy() : nullptr;
        return *this;
    }
    [[nodiscard]] const Error* getError() const { return &this->error; }
};

class ProxyResultCpp {
private:
    std::vector<ErrorCpp> errorsCppVector;
    std::vector<ProxyCpp> successfulCppVector;
    std::vector<Error> errorsVector;
    std::vector<Proxy> successfulVector;
    ProxyResult proxyResult;

public:
    ProxyResultCpp(
        const std::vector<ErrorCpp>& errorsCpp,
        const std::vector<ProxyCpp>& successfulCpp) {
        this->errorsCppVector = errorsCpp;
        this->successfulCppVector = successfulCpp;

        for (const auto& errorCpp : this->errorsCppVector) {
            this->errorsVector.push_back(*errorCpp.getError());
        }
        for (const auto& proxyCpp : this->successfulCppVector) {
            this->successfulVector.push_back(*proxyCpp.getProxy());
        }

        this->proxyResult.errors = this->errorsVector.data();
        this->proxyResult.successful = this->successfulVector.data();
        this->proxyResult.numErrors = this->errorsVector.size();
        this->proxyResult.numSuccessful = this->successfulVector.size();
    }
    [[nodiscard]] const ProxyResult* getProxyResult() const {
        return &this->proxyResult;
    }
};

class LibProxyClientApi {
private:
    class ProxyClientWrapper {
    private:
        uint64_t handle;
        std::shared_ptr<streamr::trackerlessnetwork::proxy::ProxyClient>
            proxyClient;
        std::shared_ptr<FakeTransport> fakeTransport;
        std::shared_ptr<ConnectionManager> connectionManager;
        std::atomic<int32_t> sequenceNumber = 1;

    public:
        ProxyClientWrapper(
            uint64_t handle,
            std::shared_ptr<streamr::trackerlessnetwork::proxy::ProxyClient>
                proxyClient,
            std::shared_ptr<FakeTransport> fakeTransport,
            std::shared_ptr<ConnectionManager> connectionManager)
            : handle(handle),
              proxyClient(std::move(proxyClient)),
              fakeTransport(std::move(fakeTransport)),
              connectionManager(std::move(connectionManager)) {}

        // NOLINTNEXTLINE(bugprone-exception-escape)
        ~ProxyClientWrapper() {
            // bugprone-exception-escape cannot see through the imported
            // module interfaces and assumes the stop() calls may throw
            // (same known pattern as in the import-using test files).
            this->proxyClient->stop();
            this->connectionManager->stop();
            this->fakeTransport->stop();
        }

        std::shared_ptr<streamr::trackerlessnetwork::proxy::ProxyClient>&
        getProxyClient() {
            return this->proxyClient;
        }

        std::shared_ptr<ConnectionManager>& getConnectionManager() {
            return this->connectionManager;
        }

        int32_t getNextSequenceNumber() {
            return this->sequenceNumber.fetch_add(1);
        }
    };

    FakeEnvironment fakeEnvironment;

    std::map<uint64_t, std::shared_ptr<ProxyClientWrapper>> proxyClients;
    std::recursive_mutex proxyClientsMutex;
    std::map<const ProxyResult*, std::shared_ptr<ProxyResultCpp>> results;
    std::recursive_mutex resultsMutex;

    class InvalidUrlException : public std::runtime_error {
    public:
        explicit InvalidUrlException(const std::string& message)
            : std::runtime_error(message) {}
    };

    static uint64_t createRandomHandle() { return rand(); }

    static PeerDescriptor createLocalPeerDescriptor(
        const std::string& ownEthereumAddress) {
        PeerDescriptor peerDescriptor;
        peerDescriptor.set_nodeid(
            BinaryUtils::hexToBinaryString(ownEthereumAddress));
        peerDescriptor.set_type(NodeType::NODEJS);
        peerDescriptor.set_publickey("");
        peerDescriptor.set_signature("");
        peerDescriptor.set_region(1);
        peerDescriptor.set_ipaddress(0);
        return peerDescriptor;
    }

    static PeerDescriptor createProxyPeerDescriptor(
        std::string_view proxyEthereumAddress,
        const std::string& websocketUrl) {
        PeerDescriptor proxyPeerDescriptor;

        // this will throw if the address is not valid
        auto ethereumAddress = toEthereumAddress(proxyEthereumAddress);

        proxyPeerDescriptor.set_nodeid(
            BinaryUtils::hexToBinaryString(ethereumAddress));
        proxyPeerDescriptor.set_type(NodeType::NODEJS);

        const auto parsedUrl = ada::parse<ada::url_aggregator>(websocketUrl);

        if (!parsedUrl) {
            throw InvalidUrlException(
                "Invalid websocket URL for proxy server " +
                std::string(websocketUrl));
        }

        ConnectivityMethod connectivityMethod;
        connectivityMethod.set_host(parsedUrl->get_hostname());

        if (parsedUrl->has_port()) {
            connectivityMethod.set_port(
                std::stoul(std::string(parsedUrl->get_port())));
        } else {
            if (parsedUrl->get_protocol() == "wss" ||
                parsedUrl->get_protocol() == "https") {
                connectivityMethod.set_port(443); // NOLINT
            } else {
                connectivityMethod.set_port(80); // NOLINT
            }
        }
        if (parsedUrl->get_protocol() == "wss" ||
            parsedUrl->get_protocol() == "https") {
            connectivityMethod.set_tls(true);
        } else {
            connectivityMethod.set_tls(false);
        }

        proxyPeerDescriptor.mutable_websocket()->CopyFrom(connectivityMethod);

        SLogger::trace(
            "Proxy peer descriptor created: " +
            proxyPeerDescriptor.DebugString());
        return proxyPeerDescriptor;
    }

    static std::shared_ptr<ConnectionManager> createConnectionManager(
        const DefaultConnectorFacadeOptions& opts) {
        SLogger::trace("Calling connection manager constructor");

        ConnectionManagerOptions connectionManagerOptions{
            .createConnectorFacade =
                // NOLINTNEXTLINE(bugprone-exception-escape)
            [opts]() -> std::shared_ptr<DefaultConnectorFacade> {
                return std::make_shared<DefaultConnectorFacade>(opts);
            }};
        return std::make_shared<ConnectionManager>(
            std::move(connectionManagerOptions));
    }

    const ProxyResult* addResult(
        const std::vector<ErrorCpp>& errorsCpp,
        const std::vector<ProxyCpp>& successfulCpp) {
        std::shared_ptr<ProxyResultCpp> result =
            std::make_shared<ProxyResultCpp>(errorsCpp, successfulCpp);
        std::scoped_lock lock(this->resultsMutex);
        this->results[result->getProxyResult()] = result;
        return this->results[result->getProxyResult()]->getProxyResult();
    }

public:
    void proxyClientResultDelete(const ProxyResult* proxyResult) {
        std::scoped_lock lock(this->resultsMutex);
        this->results.erase(proxyResult);
    }

    uint64_t proxyClientNew(
        const ProxyResult** proxyResult,
        const char* ownEthereumAddress, // NOLINT
        const char* streamPartId) {
        std::shared_ptr<EthereumAddress> parsedEthereumAddress;
        try {
            parsedEthereumAddress = std::make_shared<EthereumAddress>(
                toEthereumAddress(ownEthereumAddress));
        } catch (const std::runtime_error& e) {
            SLogger::error("Error in proxyClientNew: " + std::string(e.what()));

            const auto* result = addResult(
                {ErrorCpp(
                    e.what(), ERROR_INVALID_ETHEREUM_ADDRESS, std::nullopt)},
                {});
            *proxyResult = result;
            return 0;
        }
        std::shared_ptr<StreamPartID> parsedStreamPartID;
        try {
            parsedStreamPartID = std::make_shared<StreamPartID>(
                StreamPartIDUtils::parse(streamPartId));
        } catch (const std::invalid_argument& e) {
            SLogger::error("Error in proxyClientNew: " + std::string(e.what()));
            std::string message =
                "Error in proxyClientNew: " + std::string(e.what());
            *proxyResult = addResult(
                {ErrorCpp(message, ERROR_INVALID_STREAM_PART_ID, std::nullopt)},
                {});
            return 0;
        }

        auto localPeerDescriptor =
            createLocalPeerDescriptor(*parsedEthereumAddress);

        auto fakeTransport =
            this->fakeEnvironment.createTransport(localPeerDescriptor);

        auto connectionManager = createConnectionManager(
            DefaultConnectorFacadeOptions{
                .transport = *fakeTransport,
                .createLocalPeerDescriptor =
                    [localPeerDescriptor](
                        const ConnectivityResponse& /* response */)
                    -> PeerDescriptor { return localPeerDescriptor; }});
        SLogger::trace("Connection manager created, starting it");
        connectionManager->start();
        uint64_t handle = createRandomHandle();
        SLogger::trace("Creating proxy client");
        auto proxyClient = std::make_shared<ProxyClientWrapper>(
            handle,
            std::make_shared<ProxyClient>(ProxyClientOptions{
                .transport = *connectionManager,
                .localPeerDescriptor =
                    createLocalPeerDescriptor(*parsedEthereumAddress),
                .streamPartId = *parsedStreamPartID,
                .connectionLocker = *connectionManager}),
            fakeTransport,
            connectionManager);
        SLogger::trace("Proxy client created, starting it");
        proxyClient->getProxyClient()->start();
        SLogger::trace("Proxy client started");

        std::scoped_lock lock(this->proxyClientsMutex);
        this->proxyClients[handle] = proxyClient;
        *proxyResult = addResult({}, {});
        return handle;
    }

    void proxyClientDelete(
        const ProxyResult** proxyResult, uint64_t clientHandle) {
        std::scoped_lock lock(this->proxyClientsMutex);
        this->proxyClients.erase(clientHandle);
        SLogger::trace("Proxy client erased");
        *proxyResult = addResult({}, {});
    }

    uint64_t proxyClientConnect(
        const ProxyResult** proxyResult,
        uint64_t clientHandle,
        const Proxy* proxies,
        size_t numProxies) {
        if (numProxies <= 0) {
            SLogger::error("No proxies defined, returning error");
            *proxyResult = addResult(
                {ErrorCpp(
                    "No proxies defined",
                    ERROR_NO_PROXIES_DEFINED,
                    std::nullopt)},
                {});
            return 0;
        }

        std::shared_ptr<ProxyClientWrapper> proxyClient;
        {
            std::scoped_lock lock(this->proxyClientsMutex);
            auto proxyClientIterator = this->proxyClients.find(clientHandle);

            if (proxyClientIterator == this->proxyClients.end()) {
                *proxyResult = addResult(
                    {ErrorCpp(
                        "Proxy client not found with handle " +
                            std::to_string(clientHandle),
                        ERROR_PROXY_CLIENT_NOT_FOUND,
                        std::nullopt)},
                    {});
                return 0;
            }
            proxyClient = proxyClientIterator->second;
        }

        std::vector<PeerDescriptor> proxyPeerDescriptors;
        for (size_t i = 0; i < numProxies; i++) {
            const auto& proxy = proxies[i];
            try {
                proxyPeerDescriptors.push_back(createProxyPeerDescriptor(
                    proxy.ethereumAddress, proxy.websocketUrl));
            } catch (const InvalidUrlException& e) {
                *proxyResult = addResult(
                    {ErrorCpp(
                        e.what(),
                        ERROR_INVALID_PROXY_URL,
                        ProxyCpp(proxy.ethereumAddress, proxy.websocketUrl))},
                    {});
                return 0;
            } catch (const std::runtime_error& e) {
                *proxyResult = addResult(
                    {ErrorCpp(
                        e.what(),
                        ERROR_INVALID_ETHEREUM_ADDRESS,
                        ProxyCpp(proxy.ethereumAddress, proxy.websocketUrl))},
                    {});
                return 0;
            }
        }

        auto [connectionErrors, successfullyConnected] =
            proxyClient->getProxyClient()->setProxies(
                proxyPeerDescriptors,
                ProxyDirection::PUBLISH,
                proxyClient->getProxyClient()->getLocalEthereumAddress());

        std::vector<ErrorCpp> errorsCpp;
        std::vector<ProxyCpp> successfullyConnectedCpp;

        for (const auto& error : connectionErrors) {
            try {
                if (error.getOriginalException()) {
                    std::rethrow_exception(error.getOriginalException());
                } else {
                    throw std::runtime_error("No original exception");
                }
            } catch (const std::exception& e) {
                errorsCpp.emplace_back(
                    std::move(ErrorCpp(
                        e.what(),
                        ERROR_PROXY_CONNECTION_FAILED,
                        ProxyCpp(
                            "0x" +
                                Identifiers::getNodeIdFromPeerDescriptor(
                                    error.getPeerDescriptor()),
                            Connectivity::connectivityMethodToWebsocketUrl(
                                error.getPeerDescriptor().websocket())))));
            }
        }

        for (const auto& proxy : successfullyConnected) {
            successfullyConnectedCpp.emplace_back(
                std::move(ProxyCpp(
                    "0x" + Identifiers::getNodeIdFromPeerDescriptor(proxy),
                    Connectivity::connectivityMethodToWebsocketUrl(
                        proxy.websocket()))));
        }

        *proxyResult = addResult(errorsCpp, successfullyConnectedCpp);
        return successfullyConnected.size();
    }

    // Shared by proxyClientPublish and streamrNodePublish: the message
    // layout and the signature payload are identical for both APIs.
    static StreamMessage buildStreamMessage(
        std::string publisherIdHex,
        const StreamPartID& streamPartID,
        int32_t sequenceNumber,
        const char* content,
        uint64_t contentLength,
        const char* ethereumPrivateKey) {
        StreamMessage message;
        std::string contentString(content, contentLength);
        auto* contentMessage = message.mutable_contentmessage();
        contentMessage->set_content(contentString);
        contentMessage->set_contenttype(ContentType::BINARY);
        contentMessage->set_encryptiontype(EncryptionType::NONE);

        MessageID messageId;
        if (!publisherIdHex.starts_with("0x")) {
            publisherIdHex = "0x" + publisherIdHex;
        }
        messageId.set_publisherid(
            BinaryUtils::hexToBinaryString(publisherIdHex));
        messageId.set_messagechainid("1");
        messageId.set_timestamp(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count());
        messageId.set_sequencenumber(sequenceNumber);

        messageId.set_streampartition(
            static_cast<int32_t>(
                StreamPartIDUtils::getStreamPartition(streamPartID).value()));
        messageId.set_streamid(StreamPartIDUtils::getStreamID(streamPartID));

        message.mutable_messageid()->CopyFrom(messageId);

        if (ethereumPrivateKey) {
            std::string signaturePayload = messageId.streamid() +
                std::to_string(messageId.streampartition()) +
                std::to_string(messageId.timestamp()) +
                std::to_string(messageId.sequencenumber()) + publisherIdHex +
                messageId.messagechainid() + contentString;

            SLogger::trace("Signature payload: ");
            SLogger::trace("");
            SLogger::trace(
                "messageId.streamid(): \"" + messageId.streamid() + "\"");
            SLogger::trace(
                "messageId.streampartition(): \"" +
                std::to_string(messageId.streampartition()) + "\"");
            SLogger::trace(
                "messageId.timestamp(): \"" +
                std::to_string(messageId.timestamp()) + "\"");
            SLogger::trace(
                "messageId.sequencenumber(): \"" +
                std::to_string(messageId.sequencenumber()) + "\"");
            SLogger::trace("publisheridHex(): \"" + publisherIdHex + "\"");
            SLogger::trace(
                "messageId.messagechainid(): \"" + messageId.messagechainid() +
                "\"");
            SLogger::trace("contentString: \"" + contentString + "\"");
            SLogger::trace("");

            SLogger::trace(
                "Signing payload as hex string: \"" +
                BinaryUtils::binaryStringToHex(signaturePayload) + "\"");
            auto signature = SigningUtils::createSignature(
                signaturePayload, ethereumPrivateKey);

            message.set_signature(signature);
            message.set_signaturetype(SignatureType::ECDSA_SECP256K1_EVM);
            SLogger::trace(
                "Signature in hex: \"" +
                BinaryUtils::binaryStringToHex(signature) + "\"");
        }
        return message;
    }

    static StreamMessage createStreamMessage(
        const std::shared_ptr<ProxyClientWrapper>& proxyClient,
        const char* content,
        uint64_t contentLength,
        const char* ethereumPrivateKey) {
        return buildStreamMessage(
            proxyClient->getProxyClient()->getLocalEthereumAddress(),
            proxyClient->getProxyClient()->getStreamPartID(),
            proxyClient->getNextSequenceNumber(),
            content,
            contentLength,
            ethereumPrivateKey);
    }

    uint64_t proxyClientPublish(
        const ProxyResult** proxyResult,
        uint64_t clientHandle,
        const char* content,
        uint64_t contentLength,
        const char* ethereumPrivateKey) {
        std::shared_ptr<ProxyClientWrapper> proxyClient;
        {
            std::scoped_lock lock(this->proxyClientsMutex);
            auto proxyClientIterator = this->proxyClients.find(clientHandle);
            if (proxyClientIterator == this->proxyClients.end()) {
                *proxyResult = addResult(
                    {ErrorCpp(
                        "Proxy client  not found",
                        ERROR_PROXY_CLIENT_NOT_FOUND,
                        std::nullopt)},
                    {});
                return 0;
            }
            proxyClient = proxyClientIterator->second;
        }
        std::vector<ErrorCpp> errorsCpp;
        std::vector<ProxyCpp> successfullySentCpp;
        try {
            auto message = createStreamMessage(
                proxyClient, content, contentLength, ethereumPrivateKey);
            auto result = proxyClient->getProxyClient()->broadcast(message);
            for (const auto& failedPeer : result.first) {
                errorsCpp.emplace_back(
                    std::move(ErrorCpp(
                        "Failed to send message to proxy",
                        ERROR_PROXY_BROADCAST_FAILED,
                        ProxyCpp(
                            "0x" +
                                Identifiers::getNodeIdFromPeerDescriptor(
                                    failedPeer),
                            Connectivity::connectivityMethodToWebsocketUrl(
                                failedPeer.websocket())))));
            }
            for (const auto& proxy : result.second) {
                successfullySentCpp.emplace_back(
                    std::move(ProxyCpp(
                        "0x" + Identifiers::getNodeIdFromPeerDescriptor(proxy),
                        Connectivity::connectivityMethodToWebsocketUrl(
                            proxy.websocket()))));
            }
            *proxyResult = addResult(errorsCpp, successfullySentCpp);
            return successfullySentCpp.size();
        } catch (const std::exception& e) {
            SLogger::error(
                "Exception in proxyClientPublish: " + std::string(e.what()));
            errorsCpp.emplace_back(
                std::move(ErrorCpp(
                    e.what(), ERROR_PROXY_BROADCAST_FAILED, std::nullopt)));
            *proxyResult = addResult(errorsCpp, {});
            return 0;
        }
    }

    // --- streamrnode.h implementation (the full network node API; it
    // --- shares the result registry and helpers of the proxy API) ------

private:
    class StreamrNodeWrapper {
    private:
        uint64_t handle;
        std::shared_ptr<NetworkNode> networkNode;
        std::string ownEthereumAddress;
        std::atomic<int32_t> sequenceNumber = 1;
        std::atomic<bool> started{false};
        std::atomic<bool> stopped{false};
        std::map<uint64_t, HandlerToken> subscriptions;
        std::mutex subscriptionsMutex;

    public:
        StreamrNodeWrapper(
            uint64_t handle,
            std::shared_ptr<NetworkNode> networkNode,
            std::string ownEthereumAddress)
            : handle(handle),
              networkNode(std::move(networkNode)),
              ownEthereumAddress(std::move(ownEthereumAddress)) {}

        // NOLINTNEXTLINE(bugprone-exception-escape)
        ~StreamrNodeWrapper() {
            try {
                this->stop();
            } catch (const std::exception& e) {
                SLogger::error(
                    "Exception while stopping streamr node in destructor: " +
                    std::string(e.what()));
            }
        }

        // Idempotent; only ever stops a node that reached started state.
        void stop() {
            if (this->started.load() && !this->stopped.exchange(true)) {
                blockingWait(this->networkNode->stop());
            }
        }

        std::shared_ptr<NetworkNode>& getNetworkNode() {
            return this->networkNode;
        }

        [[nodiscard]] const std::string& getOwnEthereumAddress() const {
            return this->ownEthereumAddress;
        }

        [[nodiscard]] bool isStarted() const { return this->started.load(); }
        void setStarted() { this->started.store(true); }
        [[nodiscard]] bool isStopped() const { return this->stopped.load(); }
        void setStopped() { this->stopped.store(true); }

        int32_t getNextSequenceNumber() {
            return this->sequenceNumber.fetch_add(1);
        }

        void addSubscription(uint64_t subscriptionHandle, HandlerToken token) {
            std::scoped_lock lock(this->subscriptionsMutex);
            this->subscriptions.emplace(subscriptionHandle, token);
        }

        std::optional<HandlerToken> takeSubscription(
            uint64_t subscriptionHandle) {
            std::scoped_lock lock(this->subscriptionsMutex);
            auto subscription = this->subscriptions.find(subscriptionHandle);
            if (subscription == this->subscriptions.end()) {
                return std::nullopt;
            }
            auto token = subscription->second;
            this->subscriptions.erase(subscription);
            return token;
        }
    };

    std::map<uint64_t, std::shared_ptr<StreamrNodeWrapper>> streamrNodes;
    std::recursive_mutex streamrNodesMutex;

    std::shared_ptr<StreamrNodeWrapper> findStreamrNode(
        const ProxyResult** result, uint64_t nodeHandle) {
        std::scoped_lock lock(this->streamrNodesMutex);
        auto node = this->streamrNodes.find(nodeHandle);
        if (node == this->streamrNodes.end()) {
            *result = addResult(
                {ErrorCpp(
                    "Streamr node not found with handle " +
                        std::to_string(nodeHandle),
                    ERROR_NODE_NOT_FOUND,
                    std::nullopt)},
                {});
            return nullptr;
        }
        return node->second;
    }

    bool checkStreamrNodeRunning(
        const ProxyResult** result,
        const std::shared_ptr<StreamrNodeWrapper>& node) {
        if (!node->isStarted()) {
            *result = addResult(
                {ErrorCpp(
                    "The streamr node has not been started",
                    ERROR_NODE_NOT_STARTED,
                    std::nullopt)},
                {});
            return false;
        }
        if (node->isStopped()) {
            *result = addResult(
                {ErrorCpp(
                    "The streamr node has been stopped",
                    ERROR_NODE_STOPPED,
                    std::nullopt)},
                {});
            return false;
        }
        return true;
    }

    std::optional<StreamPartID> parseStreamPartIdChecked(
        const ProxyResult** result, const char* streamPartId) {
        try {
            const auto parsed = StreamPartIDUtils::parse(streamPartId);
            // Canonicalize (e.g. "#01" becomes "#1"): the delivery layer
            // keys stream parts by the canonical form derived from the
            // message's INTEGER partition, so a non-canonical key here
            // would put the subscription and the publishes of the same
            // stream part into two different (and thus empty) overlays.
            return streamr::utils::toStreamPartID(
                StreamPartIDUtils::getStreamID(parsed),
                StreamPartIDUtils::getStreamPartition(parsed).value());
        } catch (const std::invalid_argument& e) {
            *result = addResult(
                {ErrorCpp(
                    e.what(), ERROR_INVALID_STREAM_PART_ID, std::nullopt)},
                {});
            return std::nullopt;
        }
    }

    // Translates (websocketUrl, ethereumAddress) pairs to peer
    // descriptors; invalidUrlErrorCode distinguishes the entry-point and
    // proxy flavors of the same validation failure.
    std::optional<std::vector<PeerDescriptor>> parsePeerList(
        const ProxyResult** result,
        const Proxy* peers,
        uint64_t numPeers,
        const char* invalidUrlErrorCode) {
        std::vector<PeerDescriptor> peerDescriptors;
        peerDescriptors.reserve(numPeers);
        for (uint64_t i = 0; i < numPeers; i++) {
            const auto& peer = peers[i]; // NOLINT
            try {
                peerDescriptors.push_back(createProxyPeerDescriptor(
                    peer.ethereumAddress, peer.websocketUrl));
            } catch (const InvalidUrlException& e) {
                *result = addResult(
                    {ErrorCpp(
                        e.what(),
                        invalidUrlErrorCode,
                        ProxyCpp(peer.ethereumAddress, peer.websocketUrl))},
                    {});
                return std::nullopt;
            } catch (const std::runtime_error& e) {
                *result = addResult(
                    {ErrorCpp(
                        e.what(),
                        ERROR_INVALID_ETHEREUM_ADDRESS,
                        ProxyCpp(peer.ethereumAddress, peer.websocketUrl))},
                    {});
                return std::nullopt;
            }
        }
        return peerDescriptors;
    }

public:
    uint64_t streamrNodeNew(
        const ProxyResult** result,
        const char* ownEthereumAddress,
        const StreamrNodeConfig* config) {
        std::string parsedOwnAddress;
        try {
            parsedOwnAddress =
                std::string{toEthereumAddress(ownEthereumAddress)};
        } catch (const std::runtime_error& e) {
            SLogger::error("Error in streamrNodeNew: " + std::string(e.what()));
            *result = addResult(
                {ErrorCpp(
                    e.what(), ERROR_INVALID_ETHEREUM_ADDRESS, std::nullopt)},
                {});
            return 0;
        }
        const StreamrNodeConfig defaultConfig{};
        if (config == nullptr) {
            config = &defaultConfig;
        }
        auto entryPoints = parsePeerList(
            result,
            config->entryPoints,
            config->numEntryPoints,
            ERROR_INVALID_ENTRY_POINT_URL);
        if (!entryPoints.has_value()) {
            return 0;
        }

        auto localPeerDescriptor = createLocalPeerDescriptor(parsedOwnAddress);
        if (config->websocketPort != 0) {
            auto* websocket = localPeerDescriptor.mutable_websocket();
            websocket->set_host(
                config->websocketHost != nullptr ? config->websocketHost
                                                 : "127.0.0.1");
            websocket->set_port(config->websocketPort);
            websocket->set_tls(false);
        }
        if (entryPoints->empty() && config->websocketPort != 0) {
            // First node of a new network: it is its own entry point and
            // joins its own DHT (the connectivity self-check works
            // against the node's own websocket server). A node with
            // NEITHER entry points NOR a websocket server keeps an empty
            // entry point list: the connectivity check then reports the
            // local configuration without dialing anything, and
            // streamrNodeStart joins the node's own DHT explicitly.
            entryPoints->push_back(localPeerDescriptor);
        }

        auto networkNode = createNetworkNode(
            NetworkOptions{
                .layer0 =
                    DhtNodeOptions{
                        .peerDescriptor = localPeerDescriptor,
                        .entryPoints = std::move(*entryPoints)},
                .networkNode = ContentDeliveryManagerOptions{
                    .acceptProxyConnections = config->acceptProxyConnections}});

        uint64_t handle = createRandomHandle();
        auto wrapper = std::make_shared<StreamrNodeWrapper>(
            handle, std::move(networkNode), parsedOwnAddress);

        std::scoped_lock lock(this->streamrNodesMutex);
        this->streamrNodes[handle] = wrapper;
        *result = addResult({}, {});
        return handle;
    }

    void streamrNodeDelete(const ProxyResult** result, uint64_t nodeHandle) {
        std::shared_ptr<StreamrNodeWrapper> node;
        {
            // Take the wrapper out of the map but stop it OUTSIDE the
            // lock (stopping blocks on network teardown).
            std::scoped_lock lock(this->streamrNodesMutex);
            auto nodeIterator = this->streamrNodes.find(nodeHandle);
            if (nodeIterator != this->streamrNodes.end()) {
                node = nodeIterator->second;
                this->streamrNodes.erase(nodeIterator);
            }
        }
        node.reset();
        *result = addResult({}, {});
    }

    void streamrNodeStart(const ProxyResult** result, uint64_t nodeHandle) {
        auto node = findStreamrNode(result, nodeHandle);
        if (!node) {
            return;
        }
        if (node->isStopped()) {
            *result = addResult(
                {ErrorCpp(
                    "The streamr node has been stopped and cannot be "
                    "restarted",
                    ERROR_NODE_STOPPED,
                    std::nullopt)},
                {});
            return;
        }
        if (node->isStarted()) {
            *result = addResult(
                {ErrorCpp(
                    "The streamr node has already been started",
                    ERROR_NODE_ALREADY_STARTED,
                    std::nullopt)},
                {});
            return;
        }
        try {
            const auto& layer0Options =
                node->getNetworkNode()->getOptions().layer0;
            if (layer0Options.entryPoints.empty()) {
                // Isolated node (no entry points, no websocket server):
                // the doJoin path would wait for network connectivity
                // that never comes, so join the node's own DHT
                // explicitly (the pattern of the TS interop driver).
                blockingWait(node->getNetworkNode()->start(false));
                blockingWait(
                    node->getNetworkNode()
                        ->getStack()
                        .getControlLayerNode()
                        .joinDht({layer0Options.peerDescriptor.value()}));
            } else {
                blockingWait(node->getNetworkNode()->start(true));
            }
            node->setStarted();
            *result = addResult({}, {});
        } catch (const std::exception& e) {
            SLogger::error(
                "Exception in streamrNodeStart: " + std::string(e.what()));
            // Best-effort teardown of the partially started stack, then
            // mark the node unusable (start is not retryable).
            try {
                node->setStarted();
                node->stop();
            } catch (const std::exception& stopError) {
                SLogger::error(
                    "Exception while stopping partially started node: " +
                    std::string(stopError.what()));
            }
            node->setStopped();
            *result = addResult(
                {ErrorCpp(e.what(), ERROR_NODE_OPERATION_FAILED, std::nullopt)},
                {});
        }
    }

    void streamrNodeStop(const ProxyResult** result, uint64_t nodeHandle) {
        auto node = findStreamrNode(result, nodeHandle);
        if (!node) {
            return;
        }
        if (!node->isStarted()) {
            *result = addResult(
                {ErrorCpp(
                    "The streamr node has not been started",
                    ERROR_NODE_NOT_STARTED,
                    std::nullopt)},
                {});
            return;
        }
        try {
            node->stop();
            *result = addResult({}, {});
        } catch (const std::exception& e) {
            SLogger::error(
                "Exception in streamrNodeStop: " + std::string(e.what()));
            *result = addResult(
                {ErrorCpp(e.what(), ERROR_NODE_OPERATION_FAILED, std::nullopt)},
                {});
        }
    }

    void streamrNodeJoinStreamPart(
        const ProxyResult** result,
        uint64_t nodeHandle,
        const char* streamPartId,
        const StreamrEntryPoint* streamPartEntryPoints,
        uint64_t numStreamPartEntryPoints) {
        auto node = findStreamrNode(result, nodeHandle);
        if (!node || !checkStreamrNodeRunning(result, node)) {
            return;
        }
        auto parsedStreamPartId =
            parseStreamPartIdChecked(result, streamPartId);
        if (!parsedStreamPartId.has_value()) {
            return;
        }
        auto entryPoints = parsePeerList(
            result,
            streamPartEntryPoints,
            numStreamPartEntryPoints,
            ERROR_INVALID_ENTRY_POINT_URL);
        if (!entryPoints.has_value()) {
            return;
        }
        try {
            if (!entryPoints->empty()) {
                node->getNetworkNode()->setStreamPartEntryPoints(
                    *parsedStreamPartId, std::move(*entryPoints));
            }
            blockingWait(node->getNetworkNode()->join(*parsedStreamPartId));
            *result = addResult({}, {});
        } catch (const std::exception& e) {
            SLogger::error(
                "Exception in streamrNodeJoinStreamPart: " +
                std::string(e.what()));
            *result = addResult(
                {ErrorCpp(e.what(), ERROR_NODE_OPERATION_FAILED, std::nullopt)},
                {});
        }
    }

    void streamrNodeLeaveStreamPart(
        const ProxyResult** result,
        uint64_t nodeHandle,
        const char* streamPartId) {
        auto node = findStreamrNode(result, nodeHandle);
        if (!node || !checkStreamrNodeRunning(result, node)) {
            return;
        }
        auto parsedStreamPartId =
            parseStreamPartIdChecked(result, streamPartId);
        if (!parsedStreamPartId.has_value()) {
            return;
        }
        try {
            blockingWait(node->getNetworkNode()->leave(*parsedStreamPartId));
            *result = addResult({}, {});
        } catch (const std::exception& e) {
            SLogger::error(
                "Exception in streamrNodeLeaveStreamPart: " +
                std::string(e.what()));
            *result = addResult(
                {ErrorCpp(e.what(), ERROR_NODE_OPERATION_FAILED, std::nullopt)},
                {});
        }
    }

    void streamrNodePublish(
        const ProxyResult** result,
        uint64_t nodeHandle,
        const char* streamPartId,
        const char* content,
        uint64_t contentLength,
        const char* ethereumPrivateKey) {
        auto node = findStreamrNode(result, nodeHandle);
        if (!node || !checkStreamrNodeRunning(result, node)) {
            return;
        }
        auto parsedStreamPartId =
            parseStreamPartIdChecked(result, streamPartId);
        if (!parsedStreamPartId.has_value()) {
            return;
        }
        try {
            auto message = buildStreamMessage(
                node->getOwnEthereumAddress(),
                *parsedStreamPartId,
                node->getNextSequenceNumber(),
                content,
                contentLength,
                ethereumPrivateKey);
            blockingWait(node->getNetworkNode()->broadcast(message));
            *result = addResult({}, {});
        } catch (const std::exception& e) {
            SLogger::error(
                "Exception in streamrNodePublish: " + std::string(e.what()));
            *result = addResult(
                {ErrorCpp(e.what(), ERROR_NODE_OPERATION_FAILED, std::nullopt)},
                {});
        }
    }

    uint64_t streamrNodeSubscribe(
        const ProxyResult** result,
        uint64_t nodeHandle,
        const char* streamPartId,
        StreamrNodeMessageCallback callback,
        void* userData) {
        auto node = findStreamrNode(result, nodeHandle);
        if (!node || !checkStreamrNodeRunning(result, node)) {
            return 0;
        }
        if (callback == nullptr) {
            *result = addResult(
                {ErrorCpp(
                    "The message callback must not be NULL",
                    ERROR_NODE_OPERATION_FAILED,
                    std::nullopt)},
                {});
            return 0;
        }
        auto parsedStreamPartId =
            parseStreamPartIdChecked(result, streamPartId);
        if (!parsedStreamPartId.has_value()) {
            return 0;
        }
        try {
            blockingWait(node->getNetworkNode()->join(*parsedStreamPartId));
            const std::string streamPartIdString{*parsedStreamPartId};
            const std::string streamId{
                StreamPartIDUtils::getStreamID(*parsedStreamPartId)};
            const auto streamPartition = static_cast<int32_t>(
                StreamPartIDUtils::getStreamPartition(*parsedStreamPartId)
                    .value());
            auto token = node->getNetworkNode()->addMessageListener(
                // The manager emits every received message; filter to the
                // subscribed stream part here. Runs on an internal
                // network thread (documented in streamrnode.h).
                [nodeHandle,
                 streamPartIdString,
                 streamId,
                 streamPartition,
                 callback,
                 userData](const StreamMessage& message) {
                    if (message.messageid().streamid() != streamId ||
                        message.messageid().streampartition() !=
                            streamPartition ||
                        !message.has_contentmessage()) {
                        return;
                    }
                    const auto& messageContent =
                        message.contentmessage().content();
                    callback(
                        nodeHandle,
                        streamPartIdString.c_str(),
                        messageContent.data(),
                        messageContent.size(),
                        userData);
                });
            uint64_t subscriptionHandle = createRandomHandle();
            node->addSubscription(subscriptionHandle, token);
            *result = addResult({}, {});
            return subscriptionHandle;
        } catch (const std::exception& e) {
            SLogger::error(
                "Exception in streamrNodeSubscribe: " + std::string(e.what()));
            *result = addResult(
                {ErrorCpp(e.what(), ERROR_NODE_OPERATION_FAILED, std::nullopt)},
                {});
            return 0;
        }
    }

    void streamrNodeUnsubscribe(
        const ProxyResult** result,
        uint64_t nodeHandle,
        uint64_t subscriptionHandle) {
        auto node = findStreamrNode(result, nodeHandle);
        if (!node) {
            return;
        }
        auto token = node->takeSubscription(subscriptionHandle);
        if (!token.has_value()) {
            *result = addResult(
                {ErrorCpp(
                    "Subscription not found with handle " +
                        std::to_string(subscriptionHandle),
                    ERROR_SUBSCRIPTION_NOT_FOUND,
                    std::nullopt)},
                {});
            return;
        }
        node->getNetworkNode()->removeMessageListener(*token);
        *result = addResult({}, {});
    }

    uint64_t streamrNodeGetNeighborCount(
        const ProxyResult** result,
        uint64_t nodeHandle,
        const char* streamPartId) {
        auto node = findStreamrNode(result, nodeHandle);
        if (!node || !checkStreamrNodeRunning(result, node)) {
            return 0;
        }
        auto parsedStreamPartId =
            parseStreamPartIdChecked(result, streamPartId);
        if (!parsedStreamPartId.has_value()) {
            return 0;
        }
        auto neighborCount =
            node->getNetworkNode()->getNeighbors(*parsedStreamPartId).size();
        *result = addResult({}, {});
        return neighborCount;
    }

    void streamrNodeSetProxies(
        const ProxyResult** result,
        uint64_t nodeHandle,
        const char* streamPartId,
        const Proxy* proxies,
        uint64_t numProxies,
        StreamrProxyDirection direction,
        uint64_t connectionCount) {
        auto node = findStreamrNode(result, nodeHandle);
        if (!node || !checkStreamrNodeRunning(result, node)) {
            return;
        }
        auto parsedStreamPartId =
            parseStreamPartIdChecked(result, streamPartId);
        if (!parsedStreamPartId.has_value()) {
            return;
        }
        if (direction != STREAMR_PROXY_DIRECTION_PUBLISH &&
            direction != STREAMR_PROXY_DIRECTION_SUBSCRIBE) {
            *result = addResult(
                {ErrorCpp(
                    "Invalid proxy direction " +
                        std::to_string(static_cast<int>(direction)),
                    ERROR_NODE_OPERATION_FAILED,
                    std::nullopt)},
                {});
            return;
        }
        auto proxyPeerDescriptors =
            parsePeerList(result, proxies, numProxies, ERROR_INVALID_PROXY_URL);
        if (!proxyPeerDescriptors.has_value()) {
            return;
        }
        const auto proxyDirection = direction == STREAMR_PROXY_DIRECTION_PUBLISH
            ? ProxyDirection::PUBLISH
            : ProxyDirection::SUBSCRIBE;
        try {
            blockingWait(node->getNetworkNode()->setProxies(
                *parsedStreamPartId,
                std::move(*proxyPeerDescriptors),
                proxyDirection,
                toEthereumAddress(node->getOwnEthereumAddress()),
                connectionCount == 0 ? std::nullopt
                                     : std::optional<size_t>{connectionCount}));
            *result = addResult({}, {});
        } catch (const std::exception& e) {
            SLogger::error(
                "Exception in streamrNodeSetProxies: " + std::string(e.what()));
            *result = addResult(
                {ErrorCpp(e.what(), ERROR_NODE_OPERATION_FAILED, std::nullopt)},
                {});
        }
    }
};

} // namespace streamr::libstreamrproxyclient

using streamr::libstreamrproxyclient::LibProxyClientApi;

const char* testRpc() {
    return "testRpc() called";
}

static void initFolly() { // NOLINT
    folly::SingletonVault::singleton()->registrationComplete();
}

static LibProxyClientApi* libProxyClientApi = nullptr; // NOLINT

static void initialize() { // NOLINT
    // std::cout << "initialize()" << "\n";
    streamrInitLibrary();
}

void streamrInitLibrary() { // NOLINT
    if (libProxyClientApi != nullptr) {
        return;
    }
    libProxyClientApi = new LibProxyClientApi();
}

void streamrCleanupLibrary() { // NOLINT
    if (libProxyClientApi == nullptr) {
        return;
    }
    delete libProxyClientApi;
    libProxyClientApi = nullptr;
}

static LibProxyClientApi& getProxyClientApi() { // NOLINT
    return *libProxyClientApi;
}

void streamrResultDelete(const StreamrResult* streamrResult) {
    getProxyClientApi().proxyClientResultDelete(streamrResult);
}

// Deprecated pre-3.0 symbols, kept exported for one release (see
// streamrproxyclient.h). Defined here so existing binaries keep linking;
// the pragma silences the self-referential deprecation warnings.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
void proxyClientInitLibrary() { // NOLINT
    streamrInitLibrary();
}

void proxyClientCleanupLibrary() { // NOLINT
    streamrCleanupLibrary();
}

void proxyClientResultDelete(const StreamrResult* proxyResult) {
    streamrResultDelete(proxyResult);
}
#pragma clang diagnostic pop

uint64_t proxyClientNew(
    const ProxyResult** proxyResult,
    const char* ownEthereumAddress,
    const char* streamPartId) {
    initFolly(); // this can be safely called multiple times
    return getProxyClientApi().proxyClientNew(
        proxyResult, ownEthereumAddress, streamPartId);
}

void proxyClientDelete(const ProxyResult** proxyResult, uint64_t clientHandle) {
    getProxyClientApi().proxyClientDelete(proxyResult, clientHandle);
}

uint64_t proxyClientConnect(
    const ProxyResult** proxyResult,
    uint64_t clientHandle,
    const Proxy* proxies,
    uint64_t numProxies) {
    return getProxyClientApi().proxyClientConnect(
        proxyResult, clientHandle, proxies, numProxies);
}

uint64_t proxyClientPublish(
    const ProxyResult** proxyResult,
    uint64_t clientHandle,
    const char* content,
    uint64_t contentLength,
    const char* ethereumPrivateKey) {
    return getProxyClientApi().proxyClientPublish(
        proxyResult, clientHandle, content, contentLength, ethereumPrivateKey);
}

// --- streamrnode.h C shims ---------------------------------------------

uint64_t streamrNodeNew(
    const ProxyResult** result,
    const char* ownEthereumAddress,
    const StreamrNodeConfig* config) {
    initFolly(); // this can be safely called multiple times
    return getProxyClientApi().streamrNodeNew(
        result, ownEthereumAddress, config);
}

void streamrNodeDelete(const ProxyResult** result, uint64_t nodeHandle) {
    getProxyClientApi().streamrNodeDelete(result, nodeHandle);
}

void streamrNodeStart(const ProxyResult** result, uint64_t nodeHandle) {
    getProxyClientApi().streamrNodeStart(result, nodeHandle);
}

void streamrNodeStop(const ProxyResult** result, uint64_t nodeHandle) {
    getProxyClientApi().streamrNodeStop(result, nodeHandle);
}

void streamrNodeJoinStreamPart(
    const ProxyResult** result,
    uint64_t nodeHandle,
    const char* streamPartId,
    const StreamrEntryPoint* streamPartEntryPoints,
    uint64_t numStreamPartEntryPoints) {
    getProxyClientApi().streamrNodeJoinStreamPart(
        result,
        nodeHandle,
        streamPartId,
        streamPartEntryPoints,
        numStreamPartEntryPoints);
}

void streamrNodeLeaveStreamPart(
    const ProxyResult** result, uint64_t nodeHandle, const char* streamPartId) {
    getProxyClientApi().streamrNodeLeaveStreamPart(
        result, nodeHandle, streamPartId);
}

void streamrNodePublish(
    const ProxyResult** result,
    uint64_t nodeHandle,
    const char* streamPartId,
    const char* content,
    uint64_t contentLength,
    const char* ethereumPrivateKey) {
    getProxyClientApi().streamrNodePublish(
        result,
        nodeHandle,
        streamPartId,
        content,
        contentLength,
        ethereumPrivateKey);
}

uint64_t streamrNodeSubscribe(
    const ProxyResult** result,
    uint64_t nodeHandle,
    const char* streamPartId,
    StreamrNodeMessageCallback callback,
    void* userData) {
    return getProxyClientApi().streamrNodeSubscribe(
        result, nodeHandle, streamPartId, callback, userData);
}

void streamrNodeUnsubscribe(
    const ProxyResult** result,
    uint64_t nodeHandle,
    uint64_t subscriptionHandle) {
    getProxyClientApi().streamrNodeUnsubscribe(
        result, nodeHandle, subscriptionHandle);
}

uint64_t streamrNodeGetNeighborCount(
    const ProxyResult** result, uint64_t nodeHandle, const char* streamPartId) {
    return getProxyClientApi().streamrNodeGetNeighborCount(
        result, nodeHandle, streamPartId);
}

void streamrNodeSetProxies(
    const ProxyResult** result,
    uint64_t nodeHandle,
    const char* streamPartId,
    const Proxy* proxies,
    uint64_t numProxies,
    StreamrProxyDirection direction,
    uint64_t connectionCount) {
    getProxyClientApi().streamrNodeSetProxies(
        result,
        nodeHandle,
        streamPartId,
        proxies,
        numProxies,
        direction,
        connectionCount);
}
