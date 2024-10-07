#ifndef LIB_PROXY_CLIENT_API_HPP
#define LIB_PROXY_CLIENT_API_HPP

#include <ada.h>
#include <stdint.h> // NOLINT
#include <chrono>
#include <cstdlib>
#include <map>
#include <string>
#include <string_view>
#include "streamr-dht/connection/ConnectionManager.hpp"
#include "streamr-dht/connection/ConnectorFacade.hpp"
#include "streamr-dht/transport/FakeTransport.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-trackerless-network/logic/proxy/ProxyClient.hpp"
#include "streamr-utils/BinaryUtils.hpp"
#include "streamr-utils/EthereumAddress.hpp"
#include "streamr-utils/StreamPartID.hpp"
#include "streamrproxyclient.h"
namespace streamr::libstreamrproxyclient {

using ::dht::ConnectivityMethod;
using ::dht::ConnectivityResponse;
using ::dht::NodeType;
using ::dht::PeerDescriptor;
using streamr::dht::connection::ConnectionManager;
using streamr::dht::connection::ConnectionManagerOptions;
using streamr::dht::connection::DefaultConnectorFacade;
using streamr::dht::connection::DefaultConnectorFacadeOptions;
using streamr::dht::transport::FakeEnvironment;
using streamr::dht::transport::FakeTransport;
using streamr::logger::SLogger;
using streamr::trackerlessnetwork::proxy::ProxyClient;
using streamr::trackerlessnetwork::proxy::ProxyClientOptions;
using streamr::utils::BinaryUtils;
using streamr::utils::EthereumAddress;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::toEthereumAddress;
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
    std::vector<Error> errors;
    std::vector<std::string> errorMessages;
    std::recursive_mutex mutex;
    std::map<uint64_t, std::shared_ptr<ProxyClientWrapper>> proxyClients;

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

        SLogger::info(
            "Proxy peer descriptor created: " +
            proxyPeerDescriptor.DebugString());
        return proxyPeerDescriptor;
    }

    static std::shared_ptr<ConnectionManager> createConnectionManager(
        const DefaultConnectorFacadeOptions& opts) {
        SLogger::info("Calling connection manager constructor");

        ConnectionManagerOptions connectionManagerOptions{
            .createConnectorFacade =
                [opts]() -> std::shared_ptr<DefaultConnectorFacade> {
                return std::make_shared<DefaultConnectorFacade>(opts);
            }};
        return std::make_shared<ConnectionManager>(
            std::move(connectionManagerOptions));
    }

    void addError(
        std::string_view errorMessage, const char* errorCode) { // NOLINT
        std::scoped_lock lock(this->mutex);
        this->errorMessages.emplace_back(errorMessage);
        this->errors.push_back(Error{
            .message = this->errorMessages.back().c_str(), .code = errorCode});
    }

public:
    uint64_t proxyClientNew(
        Error** errors,
        uint64_t* numErrors,
        const char* ownEthereumAddress, // NOLINT
        const char* streamPartId) {
        this->errors.clear();
        this->errorMessages.clear();
        std::shared_ptr<EthereumAddress> parsedEthereumAddress;
        try {
            parsedEthereumAddress = std::make_shared<EthereumAddress>(
                toEthereumAddress(ownEthereumAddress));
        } catch (const std::runtime_error& e) {
            SLogger::error("Error in proxyClientNew: " + std::string(e.what()));
            this->errors.clear();
            this->errorMessages.clear();
            this->addError(e.what(), ERROR_INVALID_ETHEREUM_ADDRESS);
            *errors = this->errors.data();
            *numErrors = 1;
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
            this->errors.clear();
            this->errorMessages.clear();
            std::cout << "Error in proxyClientNew: " << e.what() << "\n";
            this->addError(e.what(), ERROR_INVALID_STREAM_PART_ID);
            *errors = this->errors.data();
            *numErrors = 1;
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
        SLogger::info("Connection manager created, starting it");
        connectionManager->start();
        uint64_t handle = createRandomHandle();
        SLogger::info("Creating proxy client");
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
        SLogger::info("Proxy client created, starting it");
        proxyClient->getProxyClient()->start();
        SLogger::info("Proxy client started");

        this->proxyClients[handle] = proxyClient;
        *errors = nullptr;
        *numErrors = 0;
        return handle;
    }

    void proxyClientDelete(
        Error** errors, uint64_t* numErrors, uint64_t clientHandle) {
        this->errors.clear();
        this->errorMessages.clear();
        this->proxyClients.erase(clientHandle);
        SLogger::info("Proxy client erased");
        *errors = nullptr;
        *numErrors = 0;
    }

    uint64_t proxyClientConnect(
        Error** errors,
        uint64_t* numErrors,
        uint64_t clientHandle,
        const Proxy* proxies,
        size_t numProxies) {
        this->errors.clear();
        this->errorMessages.clear();

        if (numProxies <= 0) {
            SLogger::error("No proxies defined, returning error");
            this->addError("No proxies defined", ERROR_NO_PROXIES_DEFINED);
            *errors = this->errors.data();
            *numErrors = 1;
            return 0;
        }

        auto proxyClient = this->proxyClients.find(clientHandle);
        if (proxyClient == this->proxyClients.end()) {
            this->addError(
                "Proxy client not found", ERROR_PROXY_CLIENT_NOT_FOUND);
            *errors = this->errors.data();
            *numErrors = 1;
            return 0;
        }

        std::vector<PeerDescriptor> proxyPeerDescriptors;
        for (size_t i = 0; i < numProxies; i++) {
            const auto& proxy = proxies[i];
            try {
                proxyPeerDescriptors.push_back(createProxyPeerDescriptor(
                    proxy.ethereumAddress, proxy.websocketUrl));
            } catch (const InvalidUrlException& e) {
                this->addError(e.what(), ERROR_INVALID_PROXY_URL);
                *errors = this->errors.data();
                *numErrors = 1;
                return 0;
            } catch (const std::runtime_error& e) {
                this->addError(e.what(), ERROR_INVALID_ETHEREUM_ADDRESS);
                *errors = this->errors.data();
                *numErrors = 1;
                return 0;
            }
        }

        auto result = proxyClient->second->getProxyClient()->setProxies(
            proxyPeerDescriptors,
            ProxyDirection::PUBLISH,
            proxyClient->second->getProxyClient()->getLocalEthereumAddress());

        if (!result.second.empty()) {
            for (const auto& error : result.second) {
                try {
                    if (error) {
                        std::rethrow_exception(error);
                    }
                } catch (const std::exception& e) {
                    this->addError(e.what(), ERROR_PROXY_CONNECTION_FAILED);
                }
            }
            *errors = this->errors.data();
            *numErrors = this->errors.size();
        }

        return result.first;
    }

    uint64_t proxyClientPublish(
        Error** errors,
        uint64_t* numErrors,
        uint64_t clientHandle,
        const char* content,
        uint64_t contentLength) {
        auto proxyClient = this->proxyClients.find(clientHandle);
        if (proxyClient == this->proxyClients.end()) {
            this->addError(
                "Proxy client not found", ERROR_PROXY_CLIENT_NOT_FOUND);
            *errors = this->errors.data();
            *numErrors = 1;
            return 0;
        }

        this->errors.clear();
        this->errorMessages.clear();

        StreamMessage message;
        std::string contentString(content, contentLength);
        message.mutable_contentmessage()->set_content(contentString);

        MessageID messageId;
        messageId.set_publisherid(BinaryUtils::hexToBinaryString(
            proxyClient->second->getProxyClient()->getLocalEthereumAddress()));
        messageId.set_messagechainid("1");
        messageId.set_timestamp(
            std::chrono::system_clock::now().time_since_epoch().count());
        messageId.set_sequencenumber(
            proxyClient->second->getNextSequenceNumber());
        message.mutable_messageid()->CopyFrom(messageId);
        try {
            auto result =
                proxyClient->second->getProxyClient()->broadcast(message);
            *errors = nullptr;
            *numErrors = 0;
            return result;
        } catch (const std::exception& e) {
            SLogger::error(
                "Error in proxyClientPublish: " + std::string(e.what()));
            this->addError(e.what(), ERROR_PROXY_BROADCAST_FAILED);
            *errors = this->errors.data();
            *numErrors = this->errors.size();
            return 0;
        }
    }
};

} // namespace streamr::libstreamrproxyclient

#endif