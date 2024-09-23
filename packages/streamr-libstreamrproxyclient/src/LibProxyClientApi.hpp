#ifndef LIB_PROXY_CLIENT_API_HPP
#define LIB_PROXY_CLIENT_API_HPP

#include <stdint.h> // NOLINT
#include <cstdlib>
#include <map>
#include <string>
#include "streamr-dht/connection/ConnectionManager.hpp"
#include "streamr-dht/connection/ConnectorFacade.hpp"
#include "streamr-dht/transport/FakeTransport.hpp"
#include "streamr-logger/SLogger.hpp"
#include "streamr-trackerless-network/logic/proxy/ProxyClient.hpp"
#include "streamr-utils/BinaryUtils.hpp"
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
using streamr::logger::SLogger;
using streamr::trackerlessnetwork::proxy::ProxyClient;
using streamr::trackerlessnetwork::proxy::ProxyClientOptions;
using streamr::utils::BinaryUtils;
using streamr::utils::StreamPartID;

class LibProxyClientApi {
private:
    std::vector<Error> errors;

    std::map<
        uint64_t,
        std::shared_ptr<streamr::trackerlessnetwork::proxy::ProxyClient>>
        proxyClients; // NOLINT

    static uint64_t createRandomHandle() { return rand(); }

    static PeerDescriptor createLocalPeerDescriptor(
        std::string_view ownEthereumAddress) {
        PeerDescriptor peerDescriptor;
        peerDescriptor.set_nodeid(ownEthereumAddress);
        peerDescriptor.set_type(NodeType::NODEJS);
        peerDescriptor.set_publickey("");
        peerDescriptor.set_signature("");
        peerDescriptor.set_region(1);
        peerDescriptor.set_ipaddress(0);
        return peerDescriptor;
    }

    static PeerDescriptor createProxyPeerDescriptor() {
        PeerDescriptor proxyPeerDescriptor;

        proxyPeerDescriptor.set_nodeid(BinaryUtils::hexToBinaryString(
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
        proxyPeerDescriptor.set_type(NodeType::NODEJS);
        ConnectivityMethod connectivityMethod;
        connectivityMethod.set_host("127.0.0.1");
        connectivityMethod.set_port(44211); // NOLINT
        connectivityMethod.set_tls(false);

        proxyPeerDescriptor.mutable_websocket()->CopyFrom(connectivityMethod);

        return proxyPeerDescriptor;
    }

    static std::shared_ptr<ConnectionManager> createConnectionManager(
        const DefaultConnectorFacadeOptions& opts) {
        SLogger::info("Calling connection manager constructor");

        ConnectionManagerOptions connectionManagerOptions{
            .createConnectorFacade =
                [&opts]() -> std::shared_ptr<DefaultConnectorFacade> {
                return std::make_shared<DefaultConnectorFacade>(opts);
            }};
        return std::make_shared<ConnectionManager>(
            std::move(connectionManagerOptions));
    }

public:
    uint64_t proxyClientNew(
        Error* error,
        uint64_t* numErrors,
        const char* ownEthereumAddress,
        const char* streamPartId) {
        auto localPeerDescriptor =
            createLocalPeerDescriptor(ownEthereumAddress);
        FakeEnvironment fakeEnvironment;

        auto fakeTransport =
            fakeEnvironment.createTransport(localPeerDescriptor);

        auto connectionManager =
            createConnectionManager(DefaultConnectorFacadeOptions{
                .transport = *fakeTransport,
                .createLocalPeerDescriptor =
                    [&localPeerDescriptor](
                        const ConnectivityResponse& /* response */)
                    -> PeerDescriptor { return localPeerDescriptor; }});

        SLogger::info("Starting connection manager");
        connectionManager->start();

        uint64_t handle = createRandomHandle();

        proxyClients[handle] = std::make_shared<ProxyClient>(ProxyClientOptions{
            .transport = *connectionManager,
            .localPeerDescriptor =
                createLocalPeerDescriptor(ownEthereumAddress),
            .streamPartId = StreamPartID{streamPartId},
            .connectionLocker = *connectionManager});

        if (!this->errors.empty()) {
            error = this->errors.data();
            *numErrors = this->errors.size();
            return 0;
        }

        error = nullptr;
        *numErrors = 0;
        return handle;
    }

    void proxyClientDelete(
        Error* error, uint64_t* numErrors, uint64_t clientHandle) {
        this->errors.clear();

        this->proxyClients.erase(clientHandle);

        if (!this->errors.empty()) {
            error = this->errors.data();
            *numErrors = this->errors.size();
            return;
        }

        error = nullptr;
        *numErrors = 0;
    }
};

} // namespace streamr::libstreamrproxyclient

#endif