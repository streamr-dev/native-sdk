#ifndef LIB_PROXY_CLIENT_API_HPP
#define LIB_PROXY_CLIENT_API_HPP

#include <ada.h>
#include <stdint.h> // NOLINT
#include <chrono>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/ConnectionManager.hpp"
#include "streamr-dht/connection/ConnectorFacade.hpp"
#include "streamr-dht/helpers/Connectivity.hpp"
#include "streamr-dht/transport/FakeTransport.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-trackerless-network/logic/proxy/ProxyClient.hpp"
#include "streamr-utils/BinaryUtils.hpp"
#include "streamr-utils/EthereumAddress.hpp"
#include "streamr-utils/SigningUtils.hpp"
#include "streamr-utils/StreamPartID.hpp"
#include "streamrproxyclient.h"
namespace streamr::libstreamrproxyclient {

using ::dht::ConnectivityMethod;
using ::dht::ConnectivityResponse;
using ::dht::NodeType;
using ::dht::PeerDescriptor;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::connection::ConnectionManager;
using streamr::dht::connection::ConnectionManagerOptions;
using streamr::dht::connection::DefaultConnectorFacade;
using streamr::dht::connection::DefaultConnectorFacadeOptions;
using streamr::dht::helpers::Connectivity;
using streamr::dht::transport::FakeEnvironment;
using streamr::dht::transport::FakeTransport;
using streamr::logger::SLogger;
using streamr::trackerlessnetwork::proxy::ConnectingToProxyError;
using streamr::trackerlessnetwork::proxy::ProxyClient;
using streamr::trackerlessnetwork::proxy::ProxyClientOptions;
using streamr::utils::BinaryUtils;
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

        ~ProxyClientWrapper() {
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

        auto connectionManager =
            createConnectionManager(DefaultConnectorFacadeOptions{
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
                errorsCpp.emplace_back(std::move(ErrorCpp(
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
            successfullyConnectedCpp.emplace_back(std::move(ProxyCpp(
                "0x" + Identifiers::getNodeIdFromPeerDescriptor(proxy),
                Connectivity::connectivityMethodToWebsocketUrl(
                    proxy.websocket()))));
        }

        *proxyResult = addResult(errorsCpp, successfullyConnectedCpp);
        return successfullyConnected.size();
    }

    static StreamMessage createStreamMessage(
        const std::shared_ptr<ProxyClientWrapper>& proxyClient,
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
        std::string publisherIdHex =
            proxyClient->getProxyClient()->getLocalEthereumAddress();

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
        messageId.set_sequencenumber(proxyClient->getNextSequenceNumber());

        auto streamPartID = proxyClient->getProxyClient()->getStreamPartID();
        messageId.set_streampartition(static_cast<int32_t>(
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
            message.set_signaturetype(SignatureType::SECP256K1);
            SLogger::trace(
                "Signature in hex: \"" +
                BinaryUtils::binaryStringToHex(signature) + "\"");
        }
        return message;
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
                errorsCpp.emplace_back(std::move(ErrorCpp(
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
                successfullySentCpp.emplace_back(std::move(ProxyCpp(
                    "0x" + Identifiers::getNodeIdFromPeerDescriptor(proxy),
                    Connectivity::connectivityMethodToWebsocketUrl(
                        proxy.websocket()))));
            }
            *proxyResult = addResult(errorsCpp, successfullySentCpp);
            return successfullySentCpp.size();
        } catch (const std::exception& e) {
            SLogger::error(
                "Exception in proxyClientPublish: " + std::string(e.what()));
            errorsCpp.emplace_back(std::move(ErrorCpp(
                e.what(), ERROR_PROXY_BROADCAST_FAILED, std::nullopt)));
            *proxyResult = addResult(errorsCpp, {});
            return 0;
        }
    }
};

} // namespace streamr::libstreamrproxyclient

#endif