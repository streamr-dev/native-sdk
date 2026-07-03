// Coarse façade partition over ALL public headers of
// streamr-trackerless-network (see the Phase 2.4 bench checkpoint for why
// one partition instead of one per header).
module;

#include "streamr-trackerless-network/logic/ContentDeliveryRpcLocal.hpp"
#include "streamr-trackerless-network/logic/ContentDeliveryRpcRemote.hpp"
#include "streamr-trackerless-network/logic/DuplicateMessageDetector.hpp"
#include "streamr-trackerless-network/logic/NodeList.hpp"
#include "streamr-trackerless-network/logic/Utils.hpp"
#include "streamr-trackerless-network/logic/formStreamPartDeliveryServiceId.hpp"
#include "streamr-trackerless-network/logic/propagation/FifoMapWithTTL.hpp"
#include "streamr-trackerless-network/logic/propagation/Propagation.hpp"
#include "streamr-trackerless-network/logic/propagation/PropagationTaskStore.hpp"
#include "streamr-trackerless-network/logic/propagation/RandomAccessQueue.hpp"
#include "streamr-trackerless-network/logic/proxy/ProxyClient.hpp"
#include "streamr-trackerless-network/logic/proxy/ProxyConnectionRpcLocal.hpp"
#include "streamr-trackerless-network/logic/proxy/ProxyConnectionRpcRemote.hpp"

export module streamr.trackerlessnetwork:all;

export namespace streamr::trackerlessnetwork {

using streamr::trackerlessnetwork::ContentDeliveryRpc;
using streamr::trackerlessnetwork::ContentDeliveryRpcClient;
using streamr::trackerlessnetwork::ContentDeliveryRpcLocal;
using streamr::trackerlessnetwork::ContentDeliveryRpcLocalOptions;
using streamr::trackerlessnetwork::ContentDeliveryRpcRemote;
using streamr::trackerlessnetwork::DuplicateMessageDetector;
using streamr::trackerlessnetwork::formStreamPartContentDeliveryServiceId;
using streamr::trackerlessnetwork::GapMisMatchError;
using streamr::trackerlessnetwork::InvalidNumberingError;
using streamr::trackerlessnetwork::NodeAdded;
using streamr::trackerlessnetwork::NodeList;
using streamr::trackerlessnetwork::NodeListEvents;
using streamr::trackerlessnetwork::NodeRemoved;
using streamr::trackerlessnetwork::NumberPair;
using streamr::trackerlessnetwork::Utils;

} // namespace streamr::trackerlessnetwork

export namespace streamr::trackerlessnetwork::propagation {

using streamr::trackerlessnetwork::propagation::DEFAULT_MAX_MESSAGES;
using streamr::trackerlessnetwork::propagation::DEFAULT_TTL;
using streamr::trackerlessnetwork::propagation::FifoMapWithTTL;
using streamr::trackerlessnetwork::propagation::FifoMapWithTtlOptions;
using streamr::trackerlessnetwork::propagation::Propagation;
using streamr::trackerlessnetwork::propagation::PropagationOptions;
using streamr::trackerlessnetwork::propagation::PropagationTask;
using streamr::trackerlessnetwork::propagation::PropagationTaskStore;
using streamr::trackerlessnetwork::propagation::QueueToken;
using streamr::trackerlessnetwork::propagation::RandomAccessQueue;
using streamr::trackerlessnetwork::propagation::SendToNeighborFn;

} // namespace streamr::trackerlessnetwork::propagation

export namespace streamr::trackerlessnetwork::proxy {

using streamr::trackerlessnetwork::proxy::ConnectingToProxyError;
using streamr::trackerlessnetwork::proxy::Message;
using streamr::trackerlessnetwork::proxy::NewConnection;
using streamr::trackerlessnetwork::proxy::ProxyClient;
using streamr::trackerlessnetwork::proxy::ProxyClientEvents;
using streamr::trackerlessnetwork::proxy::ProxyClientOptions;
using streamr::trackerlessnetwork::proxy::ProxyConnectionRpc;
using streamr::trackerlessnetwork::proxy::ProxyConnectionRpcClient;
using streamr::trackerlessnetwork::proxy::ProxyConnectionRpcLocal;
using streamr::trackerlessnetwork::proxy::ProxyConnectionRpcLocalEvents;
using streamr::trackerlessnetwork::proxy::ProxyConnectionRpcLocalOptions;
using streamr::trackerlessnetwork::proxy::ProxyConnectionRpcRemote;
using streamr::trackerlessnetwork::proxy::ProxyDefinition;

} // namespace streamr::trackerlessnetwork::proxy
