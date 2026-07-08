// Module streamr.dht.RecursiveOperationSessionRpcRemote
// Ported from packages/dht/src/dht/recursive-operation/
// RecursiveOperationSessionRpcRemote.ts (v103.8.0-rc.3). The client used by
// a hop to report a recursive operation's result back to the initiator's
// session (fire-and-forget sendResponse).
module;

#include <coroutine> // IWYU pragma: keep

#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

export module streamr.dht.RecursiveOperationSessionRpcRemote;

import streamr.dht.protos;

import streamr.utils.CoroutineHelper;
import streamr.logger.SLogger;
import streamr.dht.DhtRpcClient;
import streamr.dht.DhtCallContext;
import streamr.dht.RpcRemote;

// Hoisted from the former header (file scope, NOT exported).
using streamr::logger::SLogger;

export namespace streamr::dht::recursiveoperation {

using ::dht::DataEntry;
using ::dht::PeerDescriptor;
using ::dht::RecursiveOperationResponse;
using streamr::dht::contact::RpcRemote;
using streamr::dht::rpcprotocol::DhtCallContext;

using RecursiveOperationSessionRpcClient =
    ::dht::RecursiveOperationSessionRpcClient<DhtCallContext>;

class RecursiveOperationSessionRpcRemote
    : public RpcRemote<RecursiveOperationSessionRpcClient> {
public:
    RecursiveOperationSessionRpcRemote(
        PeerDescriptor localPeerDescriptor, // NOLINT
        PeerDescriptor remotePeerDescriptor,
        RecursiveOperationSessionRpcClient client,
        std::optional<std::chrono::milliseconds> timeout = std::nullopt)
        : RpcRemote<RecursiveOperationSessionRpcClient>(
              std::move(localPeerDescriptor),
              std::move(remotePeerDescriptor),
              std::move(client),
              timeout) {}

    void sendResponse(
        const std::vector<PeerDescriptor>& routingPath,
        const std::vector<PeerDescriptor>& closestConnectedNodes,
        const std::vector<DataEntry>& dataEntries,
        bool noCloserNodesFound) {
        RecursiveOperationResponse report;
        for (const auto& peer : routingPath) {
            *report.add_routingpath() = peer;
        }
        for (const auto& peer : closestConnectedNodes) {
            *report.add_closestconnectednodes() = peer;
        }
        for (const auto& entry : dataEntries) {
            *report.add_dataentries() = entry;
        }
        report.set_noclosernodesfound(noCloserNodesFound);
        auto options = this->formDhtRpcOptions();
        try {
            streamr::utils::blockingWait(this->getClient().sendResponse(
                std::move(report), std::move(options), this->getTimeout()));
        } catch (const std::exception& /*err*/) {
            SLogger::trace("Failed to send RecursiveOperationResponse");
        }
    }
};

} // namespace streamr::dht::recursiveoperation
