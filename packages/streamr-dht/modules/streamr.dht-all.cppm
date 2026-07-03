// Coarse façade partition over ALL public headers of streamr-dht.
// One partition (instead of one per header) keeps the number of
// module units — and thus repeated GMF parses of the folly/protobuf
// header stack — minimal during the façade stage. The 2.4 bench
// checkpoint showed per-header partitions multiplying build times.
// Per-header granularity returns at consolidation if needed.
module;

#include "streamr-dht/Identifiers.hpp"
#include "streamr-dht/connection/Connection.hpp"
#include "streamr-dht/connection/ConnectionLockRpcLocal.hpp"
#include "streamr-dht/connection/ConnectionLockRpcRemote.hpp"
#include "streamr-dht/connection/ConnectionLockStates.hpp"
#include "streamr-dht/connection/ConnectionLocker.hpp"
#include "streamr-dht/connection/ConnectionManager.hpp"
#include "streamr-dht/connection/ConnectionsView.hpp"
#include "streamr-dht/connection/ConnectorFacade.hpp"
#include "streamr-dht/connection/Handshaker.hpp"
#include "streamr-dht/connection/IPendingConnection.hpp"
#include "streamr-dht/connection/IncomingHandshaker.hpp"
#include "streamr-dht/connection/OutgoingHandshaker.hpp"
#include "streamr-dht/connection/PendingConnection.hpp"
#include "streamr-dht/connection/endpoint/ConnectedEndpointState.hpp"
#include "streamr-dht/connection/endpoint/ConnectingEndpointState.hpp"
#include "streamr-dht/connection/endpoint/DisconnectedEndpointState.hpp"
#include "streamr-dht/connection/endpoint/Endpoint.hpp"
#include "streamr-dht/connection/endpoint/EndpointState.hpp"
#include "streamr-dht/connection/endpoint/EndpointStateInterface.hpp"
#include "streamr-dht/connection/endpoint/InitialEndpointState.hpp"
#include "streamr-dht/connection/websocket/WebsocketClientConnection.hpp"
#include "streamr-dht/connection/websocket/WebsocketClientConnector.hpp"
#include "streamr-dht/connection/websocket/WebsocketClientConnectorRpcLocal.hpp"
#include "streamr-dht/connection/websocket/WebsocketClientConnectorRpcRemote.hpp"
#include "streamr-dht/connection/websocket/WebsocketConnection.hpp"
#include "streamr-dht/connection/websocket/WebsocketServer.hpp"
#include "streamr-dht/connection/websocket/WebsocketServerConnection.hpp"
#include "streamr-dht/connection/websocket/WebsocketServerConnector.hpp"
#include "streamr-dht/dht/contact/RpcRemote.hpp"
#include "streamr-dht/dht/routing/DuplicateDetector.hpp"
#include "streamr-dht/helpers/AddressTools.hpp"
#include "streamr-dht/helpers/CertificateHelper.hpp"
#include "streamr-dht/helpers/Connectivity.hpp"
#include "streamr-dht/helpers/Errors.hpp"
#include "streamr-dht/helpers/Offerer.hpp"
#include "streamr-dht/helpers/Version.hpp"
#include "streamr-dht/rpc-protocol/DhtCallContext.hpp"
#include "streamr-dht/transport/FakeTransport.hpp"
#include "streamr-dht/transport/ListeningRpcCommunicator.hpp"
#include "streamr-dht/transport/RoutingRpcCommunicator.hpp"
#include "streamr-dht/transport/Transport.hpp"
#include "streamr-dht/types/NatType.hpp"
#include "streamr-dht/types/PortRange.hpp"
#include "streamr-dht/types/TlsCertificateFiles.hpp"

export module streamr.dht:all;

export namespace streamr::dht {

using streamr::dht::DhtAddress;
using streamr::dht::DhtAddressRaw;
using streamr::dht::Identifiers;
using streamr::dht::kademliaIdLengthInBytes;
using streamr::dht::ServiceID;

} // namespace streamr::dht

export namespace streamr::dht::connection::connectionevents {

using streamr::dht::connection::connectionevents::Connected;
using streamr::dht::connection::connectionevents::Data;
using streamr::dht::connection::connectionevents::Disconnected;
using streamr::dht::connection::connectionevents::Error;

} // namespace streamr::dht::connection::connectionevents

export namespace streamr::dht::connection {

using streamr::dht::connection::Connection;
using streamr::dht::connection::ConnectionEvents;
using streamr::dht::connection::ConnectionID;
using streamr::dht::connection::ConnectionType;
using streamr::dht::connection::Url;

} // namespace streamr::dht::connection

export namespace streamr::dht::connection {

using streamr::dht::connection::ConnectionLockRpcLocal;
using streamr::dht::connection::ConnectionLockRpcLocalOptions;

} // namespace streamr::dht::connection

export namespace streamr::dht::connection {

using streamr::dht::connection::ConnectionLockRpcClient;
using streamr::dht::connection::ConnectionLockRpcRemote;

} // namespace streamr::dht::connection

export namespace streamr::dht::connection {

using streamr::dht::connection::ConnectionLockStates;
using streamr::dht::connection::DhtAddress;
using streamr::dht::connection::LockID;

} // namespace streamr::dht::connection

export namespace streamr::dht::connection {

using streamr::dht::connection::ConnectionLocker;

} // namespace streamr::dht::connection

export namespace streamr::dht::connection {

using streamr::dht::connection::ConnectionManager;
using streamr::dht::connection::ConnectionManagerOptions;
using streamr::dht::connection::ConnectionManagerState;

} // namespace streamr::dht::connection

export namespace streamr::dht::connection {

using streamr::dht::connection::ConnectionsView;

} // namespace streamr::dht::connection

export namespace streamr::dht::connection {

using streamr::dht::connection::ConnectorFacade;
using streamr::dht::connection::DefaultConnectorFacade;
using streamr::dht::connection::DefaultConnectorFacadeOptions;

} // namespace streamr::dht::connection

export namespace streamr::dht::connection::handshakerevents {

using streamr::dht::connection::handshakerevents::HandshakeCompleted;
using streamr::dht::connection::handshakerevents::HandshakeFailed;
using streamr::dht::connection::handshakerevents::HandshakerStopped;

} // namespace streamr::dht::connection::handshakerevents

export namespace streamr::dht::connection {

using streamr::dht::connection::Handshaker;
using streamr::dht::connection::HandshakerEvents;

} // namespace streamr::dht::connection

export namespace streamr::dht::connection::pendingconnectionevents {

using streamr::dht::connection::pendingconnectionevents::Connected;
using streamr::dht::connection::pendingconnectionevents::Disconnected;

} // namespace streamr::dht::connection::pendingconnectionevents

export namespace streamr::dht::connection {

using streamr::dht::connection::IPendingConnection;
using streamr::dht::connection::PendingConnectionEvents;

} // namespace streamr::dht::connection

export namespace streamr::dht::connection {

using streamr::dht::connection::IncomingHandshaker;

} // namespace streamr::dht::connection

export namespace streamr::dht::connection {

using streamr::dht::connection::OutgoingHandshaker;

} // namespace streamr::dht::connection

export namespace streamr::dht::connection {

using streamr::dht::connection::PendingConnection;

} // namespace streamr::dht::connection

export namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::endpoint::ConnectedEndpointState;

} // namespace streamr::dht::connection::endpoint

export namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::endpoint::ConnectingEndpointState;

} // namespace streamr::dht::connection::endpoint

export namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::endpoint::DisconnectedEndpointState;

} // namespace streamr::dht::connection::endpoint

export namespace streamr::dht::connection::endpoint::endpointevents {

using streamr::dht::connection::endpoint::endpointevents::Connected;
using streamr::dht::connection::endpoint::endpointevents::Data;
using streamr::dht::connection::endpoint::endpointevents::Disconnected;

} // namespace streamr::dht::connection::endpoint::endpointevents

export namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::endpoint::Endpoint;
using streamr::dht::connection::endpoint::EndpointEvents;

} // namespace streamr::dht::connection::endpoint

export namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::endpoint::EndpointState;

} // namespace streamr::dht::connection::endpoint

export namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::endpoint::EndpointStateInterface;

} // namespace streamr::dht::connection::endpoint

export namespace streamr::dht::connection::endpoint {

using streamr::dht::connection::endpoint::InitialEndpointState;

} // namespace streamr::dht::connection::endpoint

export namespace streamr::dht::connection::websocket {

using streamr::dht::connection::websocket::WebsocketClientConnection;

} // namespace streamr::dht::connection::websocket

export namespace streamr::dht::connection::websocket {

using streamr::dht::connection::websocket::WebsocketClientConnector;
using streamr::dht::connection::websocket::WebsocketClientConnectorOptions;

} // namespace streamr::dht::connection::websocket

export namespace streamr::dht::connection::websocket {

using streamr::dht::connection::websocket::WebsocketClientConnectorRpcLocal;
using streamr::dht::connection::websocket::
    WebsocketClientConnectorRpcLocalOptions;

} // namespace streamr::dht::connection::websocket

export namespace streamr::dht::connection::websocket {

using streamr::dht::connection::websocket::WebsocketClientConnectorRpcRemote;

} // namespace streamr::dht::connection::websocket

export namespace streamr::dht::connection::websocket {

using streamr::dht::connection::websocket::maxMessageSize;
using streamr::dht::connection::websocket::WebsocketConnection;

} // namespace streamr::dht::connection::websocket

export namespace streamr::dht::connection::websocket::websocketserverevents {

using streamr::dht::connection::websocket::websocketserverevents::Connected;

} // namespace streamr::dht::connection::websocket::websocketserverevents

export namespace streamr::dht::connection::websocket {

using streamr::dht::connection::websocket::defaultMaxMessageSize;
using streamr::dht::connection::websocket::WebsocketServer;
using streamr::dht::connection::websocket::WebsocketServerConfig;
using streamr::dht::connection::websocket::WebsocketServerEvents;

} // namespace streamr::dht::connection::websocket

export namespace streamr::dht::connection::websocket {

using streamr::dht::connection::websocket::WebsocketServerConnection;

} // namespace streamr::dht::connection::websocket

export namespace streamr::dht::connection::websocket {

using streamr::dht::connection::websocket::WebsocketServerConnector;
using streamr::dht::connection::websocket::WebsocketServerConnectorOptions;

} // namespace streamr::dht::connection::websocket

export namespace streamr::dht::contact {

using streamr::dht::contact::RpcRemote;

} // namespace streamr::dht::contact

export namespace streamr::dht::routing {

using streamr::dht::routing::DuplicateDetector;

} // namespace streamr::dht::routing

export namespace streamr::dht::helpers {

using streamr::dht::helpers::AddressTools;

} // namespace streamr::dht::helpers

export namespace streamr::dht::helpers {

using streamr::dht::helpers::ASN1_TIME_ptr;
using streamr::dht::helpers::BIO_ptr;
using streamr::dht::helpers::CertificateHelper;
using streamr::dht::helpers::rsaKeyLength;
using streamr::dht::helpers::TlsCertificate;
using streamr::dht::helpers::X509_ptr;

} // namespace streamr::dht::helpers

export namespace streamr::dht::helpers {

using streamr::dht::helpers::Connectivity;

} // namespace streamr::dht::helpers

export namespace streamr::dht::helpers {

using streamr::dht::helpers::CannotConnectToSelf;
using streamr::dht::helpers::CouldNotStart;
using streamr::dht::helpers::DhtException;
using streamr::dht::helpers::Err;
using streamr::dht::helpers::ErrorCode;
using streamr::dht::helpers::SendFailed;
using streamr::dht::helpers::WebsocketServerStartError;

} // namespace streamr::dht::helpers

export namespace streamr::dht::helpers {

using streamr::dht::helpers::Offerer;
using streamr::dht::helpers::OffererHelper;

} // namespace streamr::dht::helpers

export namespace streamr::dht::helpers {

using streamr::dht::helpers::Version;
using streamr::dht::helpers::VersionNumber;

} // namespace streamr::dht::helpers

export namespace streamr::dht::rpcprotocol {

using streamr::dht::rpcprotocol::DhtCallContext;

} // namespace streamr::dht::rpcprotocol

export namespace streamr::dht::transport {

using streamr::dht::transport::FakeEnvironment;
using streamr::dht::transport::FakeTransport;

} // namespace streamr::dht::transport

export namespace streamr::dht::transport {

using streamr::dht::transport::ListeningRpcCommunicator;

} // namespace streamr::dht::transport

export namespace streamr::dht::transport {

using streamr::dht::transport::RoutingRpcCommunicator;
using streamr::dht::transport::RpcCommunicator;

} // namespace streamr::dht::transport

export namespace streamr::dht::transport::transportevents {

using streamr::dht::transport::transportevents::Connected;
using streamr::dht::transport::transportevents::Disconnected;
using streamr::dht::transport::transportevents::Message;

} // namespace streamr::dht::transport::transportevents

export namespace streamr::dht::transport {

using streamr::dht::transport::SendOptions;
using streamr::dht::transport::Transport;
using streamr::dht::transport::TransportEvents;

} // namespace streamr::dht::transport

export namespace streamr::dht::types::NatType {

using streamr::dht::types::NatType::OPEN_INTERNET;
using streamr::dht::types::NatType::UNKNOWN;

} // namespace streamr::dht::types::NatType

export namespace streamr::dht::types {

using streamr::dht::types::PortRange;

} // namespace streamr::dht::types

export namespace streamr::dht::types {

using streamr::dht::types::TlsCertificateFiles;

} // namespace streamr::dht::types
