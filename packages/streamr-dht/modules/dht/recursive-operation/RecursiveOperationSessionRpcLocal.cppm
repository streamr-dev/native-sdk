// Module streamr.dht.RecursiveOperationSessionRpcLocal
// Ported from packages/dht/src/dht/recursive-operation/
// RecursiveOperationSessionRpcLocal.ts (v103.8.0-rc.3). The initiator's
// session registers this to receive the per-hop RecursiveOperationResponse
// reports.
module;

#include <functional>
#include <vector>
#include "packages/dht/protos/DhtRpc.pb.h"

export module streamr.dht.RecursiveOperationSessionRpcLocal;

import streamr.logger.SLogger;
import streamr.dht.DhtRpcServer;
import streamr.dht.DhtCallContext;
import streamr.dht.Identifiers;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;

export namespace streamr::dht::recursiveoperation {

using ::dht::DataEntry;
using ::dht::PeerDescriptor;
using ::dht::RecursiveOperationResponse;
using streamr::dht::DhtAddress;
using streamr::dht::Identifiers;
using streamr::dht::rpcprotocol::DhtCallContext;

using RecursiveOperationSessionRpc =
    ::dht::RecursiveOperationSessionRpc<DhtCallContext>;

struct RecursiveOperationSessionRpcLocalOptions {
    std::function<void(
        const DhtAddress& remoteNodeId,
        const std::vector<PeerDescriptor>& routingPath,
        const std::vector<PeerDescriptor>& nodes,
        const std::vector<DataEntry>& dataEntries,
        bool noCloserNodesFound)>
        onResponseReceived;
};

class RecursiveOperationSessionRpcLocal : public RecursiveOperationSessionRpc {
private:
    RecursiveOperationSessionRpcLocalOptions options;

public:
    explicit RecursiveOperationSessionRpcLocal(
        RecursiveOperationSessionRpcLocalOptions options)
        : options(std::move(options)) {}

    ~RecursiveOperationSessionRpcLocal() override = default;

    void sendResponse(
        const RecursiveOperationResponse& report,
        const DhtCallContext& callContext) override {
        const DhtAddress remoteNodeId =
            Identifiers::getNodeIdFromPeerDescriptor(
                callContext.incomingSourceDescriptor.value());
        SLogger::trace("RecursiveOperationResponse arrived");
        std::vector<PeerDescriptor> routingPath(
            report.routingpath().begin(), report.routingpath().end());
        std::vector<PeerDescriptor> closestConnectedNodes(
            report.closestconnectednodes().begin(),
            report.closestconnectednodes().end());
        std::vector<DataEntry> dataEntries(
            report.dataentries().begin(), report.dataentries().end());
        this->options.onResponseReceived(
            remoteNodeId,
            routingPath,
            closestConnectedNodes,
            dataEntries,
            report.noclosernodesfound());
    }
};

} // namespace streamr::dht::recursiveoperation
