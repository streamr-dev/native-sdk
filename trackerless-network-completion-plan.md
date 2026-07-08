# Trackerless Network Completion Plan

Plan for making the C++ port of the `dht` and `trackerless-network` packages feature-complete
with the TypeScript implementation, so that the shared library can connect **directly** to the
Streamr peer-to-peer network (as a full node) in addition to the existing proxy method.

- **Reference implementation:** `/Users/ptesavol/projects/network` at version **103.8.0-rc.3**
  (git commit `af966cf03`). All porting work should be done against this pinned commit; bump the
  pin deliberately, not implicitly.
- **Method:** test-driven, iterative porting. For each step: port a TypeScript test, port the
  minimal set of dependencies the test needs, make the test green, refactor to house style, move
  to the next test.
- **Design rule:** where the two codebases deviate in design or implementation, **the C++ version
  wins**. In particular the connection handling in `streamr-dht` (the endpoint state machine) is a
  deliberate redesign and is the architecture all new code must interface with. We port *behavior
  and test assertions* from TypeScript, not structure.

---

## 1. Where we are today

### 1.1 Already ported and tested in C++

**`streamr-dht`** — the connection layer is essentially complete:

- Redesigned endpoint state machine: `Endpoint`, `EndpointState` (+ `Initial`/`Connecting`/
  `Connected`/`Disconnected` states), `EndpointStateInterface`, `PendingConnection`,
  `IncomingHandshaker`/`OutgoingHandshaker`.
- WebSocket connectivity in both directions: `WebsocketClientConnector`,
  `WebsocketServerConnector`, `WebsocketServer`, client/server connections (built on
  libdatachannel), TLS certificate helper.
- `ConnectionManager` (implements `Transport`), `ConnectionsView`, connection locking
  (`ConnectionLocker`, `ConnectionLockStates`, `ConnectionLockRpcLocal/Remote`).
- Transport plumbing: `Transport` interface, `FakeTransport`, `RoutingRpcCommunicator`,
  `ListeningRpcCommunicator`, `DhtCallContext`.
- Helpers: `Identifiers` (DhtAddress etc.), `AddressTools`, `Connectivity`, `CertificateHelper`,
  `Version` (protocol version `1.1`, matching TypeScript), `Offerer`, `DuplicateDetector`.

**`streamr-trackerless-network`** — the proxy publish path:

- `ProxyClient`, `ProxyConnectionRpcLocal/Remote`, `ContentDeliveryRpcLocal/Remote`,
  `Propagation`, `PropagationTaskStore`, `FifoMapWithTTL`, `RandomAccessQueue`,
  `DuplicateMessageDetector`, `NodeList`, `formStreamPartDeliveryServiceId`, `Utils`.

**Supporting packages** (complete enough; extend on demand): `streamr-proto-rpc` (RPC
communicator + protoc code-generation plugin), `streamr-eventemitter`, `streamr-utils`
(AbortController, coroutine helpers, signing, StreamPartID, waitForCondition/waitForEvent, ...),
`streamr-logger`, `streamr-json`, and the `streamr-libstreamrproxyclient` shared-library C API.

### 1.2 Missing relative to TypeScript

**`dht` package** (everything above the connection layer):

| Subsystem | TypeScript classes |
|---|---|
| Contact lists | `SortedContactList`, `RandomContactList`, `RingContactList`, `Contact`, `getClosestNodes`, ring identifiers |
| Routing table | k-bucket (npm `k-bucket` library — needs a C++ port), `PeerManager` |
| Node RPC | `DhtNodeRpcLocal`, `DhtNodeRpcRemote` (ping, getClosestPeers, getClosestRingPeers, leaveNotice) |
| Routing | `Router`, `RoutingSession`, `RouterRpcLocal/Remote`, `RoutingTablesCache`, `getPreviousPeer` |
| Recursive operations | `RecursiveOperationManager`, `RecursiveOperationSession`, their RpcLocal/Remote pairs (FIND_CLOSEST_NODES / FETCH_DATA / DELETE_DATA) |
| Data store | `LocalDataStore`, `StoreManager`, `StoreRpcLocal/Remote` |
| Join / discovery | `PeerDiscovery`, `DiscoverySession`, `RingDiscoverySession` |
| Node assembly | `DhtNode` (implements the transport interface, enabling layer-0/layer-1 stacking), `ExternalApiRpcLocal/Remote`, `createPeerDescriptor` (signed peer descriptors) |
| WebRTC | `WebrtcConnector`, `WebrtcConnectorRpcLocal/Remote`, WebRTC connection (node-datachannel in TS; **libdatachannel in C++ — same underlying library**, already a vcpkg dependency) |
| Connectivity | `connectivityChecker` / `connectivityRequestHandler` (NAT probing during peer-descriptor creation) |
| Test infra | `Simulator`, `SimulatorTransport`, `SimulatorConnector/Connection` (the C++ `ConnectionType` enum already reserves `SIMULATOR_SERVER/CLIENT`) |

**`trackerless-network` package** (everything except the proxy publish path):

| Subsystem | TypeScript classes |
|---|---|
| Per-stream overlay node | `ContentDeliveryLayerNode`, `createContentDeliveryLayerNode`, node views (nearby/random/left/right) |
| Neighbor discovery | `Handshaker`, `HandshakeRpcLocal/Remote` (incl. interleaving), `NeighborFinder`, `NeighborUpdateManager`, `NeighborUpdateRpcLocal/Remote` |
| Entry points in DHT | `PeerDescriptorStoreManager`, `StreamPartNetworkSplitAvoidance`, `StreamPartReconnect` |
| Orchestration | `ContentDeliveryManager`, `NetworkStack`, `NetworkNode`, `ExternalNetworkRpc` |
| Diagnostics | `Inspector`, `InspectSession`, `TemporaryConnectionRpcLocal/Remote`, `NodeInfoRpcLocal/Remote`, `NodeInfoClient`, `GapDiagnostics` |
| Proxy server side | accepting proxy connections as a full node (`acceptProxyConnections`) |
| Optimization | `PlumtreeManager`, `PlumtreeRpcLocal/Remote`, `PausedNeighbors` |

### 1.3 Proto drift (verified by diff against 103.8.0-rc.3)

The C++ `.proto` files were copied from an earlier TypeScript version and have drifted:

- `DhtRpc.proto`: missing `ConnectionLockRpc.setPrivate`, `ExternalApiRpc.externalFindClosestNodes`,
  and messages `SetPrivateRequest`, `ExternalFindClosestNodesRequest/Response`.
- `NetworkRpc.proto`: missing the whole `PlumtreeRpc` service and its messages
  (`PauseNeighborRequest/Response`, `ResumeNeighborRequest`), missing
  `ContentDeliveryLayerNeighborInfo`, and `GroupKey` has been renamed to `EncryptedGroupKey`
  in TypeScript (a wire-visible change that affects the existing proxy path).

Re-synchronizing the protos is therefore the very first work item (phase 0).

---

## 2. Ground rules for the port

### 2.1 The test-driven loop

For every sub-phase:

1. **Pick the next TypeScript test file** from the phase's list (ordered roughly by dependency).
2. **Port the test** to gtest, preserving the behavioral assertions. File naming follows the
   existing convention: TS `test/unit/SortedContactList.test.ts` becomes
   `packages/streamr-dht/test/unit/SortedContactListTest.cpp`. Add a comment at the top of each
   ported test referencing the TypeScript source path and the pinned version, e.g.
   `// Ported from packages/dht/test/unit/SortedContactList.test.ts (v103.8.0-rc.3)`.
3. **Port the dependencies** the test needs — the minimal set, translated to house style.
4. **Make it green**, then run the full package test suite to catch regressions.
5. Move to the next test.

Adaptation rules when porting tests:

- Assertions and observable behavior are ported faithfully; internal structure follows the C++
  design. If a TS test reaches into internals that do not exist in the C++ design, rewrite it
  against the public behavior it actually verifies (and note the adaptation in the file comment).
- TypeScript tests often rely on single-threaded event-loop timing. Replace timing assumptions
  with the existing `waitForCondition` / `waitForEvent` / `runAndWaitForEvents` utilities from
  `streamr-utils`. Treat any test that only passes with added sleeps as a bug in the port.
- Jest `fakeTimers` usages need case-by-case adaptation (usually: injectable timeouts plus
  `waitForCondition`).

### 2.2 Design deviations — C++ wins

- **Connection layer:** new code interfaces with `Endpoint`/`PendingConnection`/
  `ConnectorFacade`/`ConnectionManager` as they exist in C++. The TypeScript
  `ManagedConnection`/`OutputBuffer` concepts map onto the `Endpoint` state machine (buffering
  while connecting lives in `ConnectingEndpointState`). TS connection-layer unit tests that are
  already covered by existing C++ tests are **not** re-ported; only the integration-level tests
  (e.g. `SimultaneousConnections.test.ts`, `WebsocketConnectionManagement.test.ts`) are ported,
  rewritten against the C++ API, because they verify cross-node protocol behavior.
- **Threading model:** TypeScript is single-threaded; the C++ code is multi-threaded with
  folly coroutines and explicit locking. Every ported class must state its locking discipline
  (which mutex protects what), following the pattern used by `Endpoint` and `ConnectionManager`.
- Anything else where the TS design is awkward (e.g. callback-typed config bags) is translated
  to the established C++ idioms rather than mirrored.

### 2.3 Wire compatibility — TypeScript wins

The one place where the TypeScript version is authoritative is the **wire protocol**: proto
files, RPC service names and semantics, handshake and protocol-version behavior, Kademlia ID
computation (20-byte node IDs), the `SHA1(streamPartId)` data-key derivation, and signature
payloads (`createPeerDescriptorSignaturePayload`). Interoperability with TS nodes is the
acceptance criterion for every milestone, so wire-visible behavior is ported bit-exactly and
covered by interop tests. NOTE: the repo's existing `ts-integration` harness
(`ProxyClientTsIntegrationTest`) is currently **disabled** — it builds a 2024 TS monorepo that
no longer works on CI (see the comment in `packages/streamr-trackerless-network/CMakeLists.txt`
and MODERNIZATION.md, "Interim — ts-integration tests"). Rebuilding that harness against the
pinned 103.8 version is a prerequisite for the milestone B and C interop exit criteria and
should be scheduled as its own work item early in milestone B.

### 2.4 House style and repo constraints

- C++26, existing naming conventions, header-only where the surrounding code is header-only,
  `Branded` types for identifiers, `MakeSharedEnabler` pattern for shared-ptr-owned classes,
  `streamr-eventemitter` for events, folly coroutines for async, SLogger for logging.
- Monorepo principles hold: each package must keep building standalone (vcpkg publishability),
  and everything must stay compatible with the iOS runtime libc++ gate. New third-party
  dependencies go through `vcpkg.json`/overlayports — though none are currently anticipated:
  WebRTC comes from libdatachannel (already present), and the small TS libraries map to the
  standard library (`k-bucket` → hand-ported class, `@js-sdsl/ordered-map` → `std::map`,
  `heap` → `std::priority_queue`, `lru-cache` → small local implementation, `lodash.sample` →
  `std::sample`, `uuid`/`eventemitter3`/`ws`/`node-datachannel` → already covered).
- **Process:** one pull request per sub-phase, reviewed on GitHub before the next begins.
  Every PR leaves the full monorepo test suite green.

---

## 3. Cross-cutting decisions (settle in phase 0)

1. **Network simulator for integration tests.** A large share of TS dht and
   trackerless-network integration tests run on `Simulator`/`SimulatorTransport` (an in-memory
   network hub with optional fixed or per-region latency). The C++ `FakeTransport` is too small
   a substitute — it fakes a transport, not a network of connectable nodes. We need something
   that presents the **same API surface the ported tests consume** (`Simulator`,
   `SimulatorTransport`, latency options), packaged as a `SimulatorConnectorFacade` behind the
   existing `ConnectorFacade` interface so `ConnectionManager` runs unmodified on top of it
   (this mirrors the TS structure, and the reserved `SIMULATOR_*` connection types suggest it
   was always the intent). For the implementation *behind* that API there are three options —
   note that the TS simulator is itself a very simple simulator and may contain bugs of its
   own, so fidelity to it is not automatically a virtue:

   - **(a) Use an existing simulator library.** Established network simulators (ns-3,
     OMNeT++) are packet-level discrete-event frameworks: heavyweight, not in vcpkg, awkward
     to embed in unit-test processes, and their fidelity (TCP/IP stacks, PHY models) buys
     nothing for message-level delivery between in-process nodes. No lightweight C++
     "message-level network simulator" library with real adoption is known to us. Rejected
     unless one turns up during phase 0 — the integration cost dominates any benefit.
   - **(b) Implement the simulator API from scratch (recommended).** Keep the TS-compatible
     API, but design the internals for a multi-threaded runtime instead of translating
     event-loop JavaScript: a single dispatcher thread owning all message delivery, per
     connection-pair FIFO queues (guaranteed ordering), latency modeled as scheduled delivery
     deadlines in a priority queue, and the per-region latency table taken **as data** from the
     TS source (`pings.ts`). This gives determinism by construction, avoids inheriting TS bugs,
     and is small (roughly the same few hundred lines the TS version is).
   - **(c) Port the simulator from the TS source.** Maximum behavioral fidelity to what the TS
     tests were written against, but it inherits any TS bugs, and the TS implementation
     implicitly relies on the JS event loop's single-threaded scheduling — a line-by-line
     translation into threaded C++ would likely be *less* correct, not more.

   **Recommendation: (b)**, with the API kept close enough to TS that ported tests translate
   mechanically. Two guardrails regardless of option: the simulator gets its **own unit-test
   suite** (delivery, ordering, latency, disconnect semantics) so simulator bugs cannot
   masquerade as system-under-test failures; and where a ported test's expectations turn out to
   encode a TS-simulator quirk rather than real protocol behavior, the test is adapted and the
   deviation noted in its header comment. Deliver as part of milestone 0 (phase 0.3) because
   nearly all later integration tests depend on it.
2. **k-bucket port.** TypeScript uses the npm `k-bucket` library. It is small (~300 lines) but
   its exact semantics (bucket splitting, `arbiter` conflict resolution, event emission, the
   `vectorClock` usage from `DhtNodeRpcRemote`) matter for routing-table behavior. Port it as a
   standalone class inside `streamr-dht` with its own unit tests derived from the library's
   documented behavior and `PeerManager.test.ts` expectations.
3. **Plumtree scope — RESOLVED: defer to milestone E.** `PlumtreeRpc` is an optional gossip
   optimization. Verified against the pinned TS source: it is opt-in
   (`createContentDeliveryLayerNode.ts:73` only constructs `PlumtreeManager` when
   `plumtreeOptimization` is set), and the initiating side treats a failed/unanswered
   `pauseNeighbor` call as "not accepted" via try/catch (`PlumtreeManager.ts:99–111`), so a
   peer without the service is simply never paused and content delivery proceeds normally.
   A C++ node therefore interoperates safely without implementing it; the proto definitions
   are compiled in (phase 0.1) so the service can be added later as a pure optimization.
4. **Autocertifier and GeoIP: defer.** Both matter only for publicly reachable server nodes
   (TLS certificates for the websocket server; coordinates in connectivity responses). The
   shared-library use case (mobile/embedded clients joining the network outbound) does not need
   them. Keep the existing self-signed-certificate path, mark autocertifier/GeoIP explicitly out
   of scope for v1, and leave extension points where `WebsocketServerConnector` consults them.
5. **Browser-specific TS code** (`src/browser/*`, worker WebRTC bridges, comlink) is out of
   scope permanently — it has no C++ counterpart.
6. **Suppress-own-message-loopback:** TS recently added `suppressOwnMessageLoopback`
   (commit `f9d7ad992`); include it in `ContentDeliveryManager` options when that phase arrives.

---

## 4. Phased roadmap

Phases are grouped into milestones A–E. Each sub-phase is intended to be one reviewable PR
(split further if a PR grows past ~1.5k lines of non-generated code). For each sub-phase the
TS tests to port are listed; paths are relative to the TS package's `test/` directory.

### Milestone 0 — Groundwork

**Phase 0.1 — Adopt the new TS protos.**
Take the proto files from the pinned TS commit into use wholesale — they replace the current
C++ copies, they are not merged with them. Concretely: copy `DhtRpc.proto` and
`NetworkRpc.proto` (and any transitively referenced proto) from TS 103.8.0-rc.3 verbatim,
regenerate all C++ stubs with the streamr protoc plugin, and **migrate the existing C++ code to
the new protos in this same phase** — notably the `GroupKey` → `EncryptedGroupKey` rename in
the proxy path, plus compiling (even if not yet serving) the new `PlumtreeRpc`,
`ConnectionLockRpc.setPrivate` and `ExternalApiRpc.externalFindClosestNodes` definitions. From
this phase on, the pinned TS protos are the single source of truth for the wire format; add a
CI-friendly check script that diffs the repo's protos against the pinned TS protos so any
future drift fails loudly instead of accumulating.
*Tests:* existing suites stay green on the regenerated stubs. (Live interop verification of
the renamed messages must wait for the rebuilt TS-interop harness — see the note in
section 2.3; the disabled `ProxyClientTsIntegrationTest` still has to compile against the new
protos, which the build enforces.)

**Phase 0.2 — Test-utility foundation.**
Port the small factories both TS packages lean on, into per-package `test/utils/`:
`createMockPeerDescriptor`, `createWrappedClosestPeersRequest`, `createStreamMessage`,
`mockConnectionLocker`, plus gtest custom matchers where TS uses `customMatchers.test.ts`
(port that test too). Larger factories (`createMockConnectionDhtNode`,
`createMockConnectionLayer1Node`, `createMockContentDeliveryLayerNodeAndDhtNode`) are ported
later, in the phase that first needs them.

**Phase 0.3 — Network simulator, implemented from scratch.**
Implement the test-network simulator per decision 3.1 (option b): TS-compatible API surface
(`Simulator`, `SimulatorTransport`, latency options), internals designed for the threaded C++
runtime — a single dispatcher thread owning all message delivery, per connection-pair FIFO
queues, latency modeled as scheduled delivery deadlines in a priority queue, and the per-region
latency table imported as data from TS `pings.ts`. New classes: `Simulator`,
`SimulatorConnection`, `SimulatorConnector` packaged as a `SimulatorConnectorFacade` for the
existing `ConnectionManager`; convenience alias `SimulatorTransport`.
*Tests:* first a dedicated unit suite for the simulator itself (delivery, ordering, latency,
disconnect semantics — it is test infrastructure everything else stands on), then validate it
against the real connection layer by porting `integration/ConnectionManager.test.ts` (adapted
to the C++ endpoint design) and `integration/SimultaneousConnections.test.ts`. These two ports
double as a check that the existing endpoint state machine handles the simultaneous-connect
(connection tie-breaking) cases the TS tests encode.

### Milestone A — DHT core logic (runs on fake/simulator transport, no new connectivity)

**Phase A0 — Connection-layer locking-discipline hardening.**
Added after phase 0.3: the ported simultaneous-connect tests exposed (and partially fixed)
lifetime and locking bugs in the connection layer that TypeScript's single-threaded runtime
never hits. Fixed in 0.3: discarded handler tokens in `ConnectingEndpointState`; raw-`this`
captures in the handshakers' event handlers (now weak self-captures registered after
construction); the `waitForCondition` poller lifetime. Remaining, diagnosed under
`--gtest_repeat` stress (single runs are stable): two ABBA lock-order inversions —
(1) main thread `Endpoint::mutex → state mutex` vs. dispatcher
`state mutex → Endpoint::mutex` (a connected-event handler in `ConnectingEndpointState`
drives the endpoint while a send holds it), and (2) `ConnectionManager::endpointsMutex →
Endpoint::mutex` (acceptNewConnection/setConnecting) vs. `Endpoint::mutex → endpointsMutex`
(handleDisconnect → removeSelfFromContainer). The fix is a locking-policy pass over
Endpoint/EndpointStates/ConnectionManager — one mutex per endpoint state machine, and no
call-outs (emits, container callbacks) while holding any of these locks — plus a TSAN CI leg
and `--gtest_repeat` stress runs as the acceptance test. Prerequisite for scaling the
simulator-based integration tests in the rest of milestone A.
*Implemented (phase-A0 PR):* the state machine now runs under one recursive mutex owned by
`Endpoint` (reached by the states through `EndpointStateInterface::getStateMachineMutex()`;
the per-state mutexes are gone), every public `Endpoint` operation is two-phase — transition
under the mutex, call-outs (event emits, connection/pending-connection close, container
removal, replay-emitter handler registration) after releasing it, with the states returning
their call-outs as deferred closures — and connection-event handlers pin the endpoint through
a weak reference and re-validate under the mutex that they still belong to the current
pending connection/connection before acting (an in-flight emit can outlive `off()`, and a
`ReplayEventEmitter` can replay during `on()`). `ConnectionManager` holds `endpointsMutex`
only for container lookups/inserts; `createConnection`/`onNewConnection`, `setConnecting`
and the endpoint queries in `send()`/`getConnections()` run outside it, and the
exists-check/insert race in `acceptNewConnection` is closed by deciding and inserting under
a single lock hold. The TSAN CI leg is still open (tracked as follow-up; the vcpkg-built
dependencies would need instrumented builds for a clean run).
Two further defects surfaced by the same stress loop were fixed alongside the locking pass:
(a) `EventEmitter::emit` `std::forward`-ed the arguments into every listener in its dispatch
loop, so with more than one listener the second and later handlers received moved-from
(empty) values — it now passes each listener its own copy; and (b) `waitForEvent` registered
a `once` listener that captured a stack waiter and never removed it on the timeout/cancel
path, so a later emit invoked the dangling listener and called `setValue` on the destroyed
promise (a folly `Promise.h` abort seen in the teardown of a timed-out connection) — the
waiter is now heap-owned and co-owned by the listener, which is removed on every exit path.
Both are covered by new unit tests in `streamr-eventemitter` and `streamr-utils`.
The remaining defects were in the simultaneous-connect convergence, which the threaded
runtime exposes but TS's single-threaded model hides (both surfaced as a `received2` timeout,
initially ~1 in 30 simultaneous-connect stress iterations). Three changes, all matching the
TS behavior once the threading is accounted for:
- *Direction-aware tie-break.* `acceptNewConnection` replaced on `getOfferer == remote`
  alone, correct only when a node always registers its own outgoing connection before it
  processes the peer's incoming one. The connector's dispatcher thread can deliver the
  incoming before the main-thread `send()` registers the outgoing, inverting the tie-break so
  the two nodes settle on different connections. `onNewConnection` now carries an
  `isLocalInitiated` flag and the winning connection is the offerer's regardless of which
  thread registered it first.
- *Adoption sequence number.* Even with the right decision, the initial `setConnecting` from
  `send()` and the replacing `setConnecting` from the incoming connection race on the endpoint
  mutex outside `endpointsMutex`, so the initial one could run last and re-adopt the losing
  connection. `ConnectionManager` assigns a monotonic sequence under `endpointsMutex` and the
  endpoint adopts only strictly-newer sequences, making the outcome independent of that race.
- *No teardown of the losing endpoint.* `OutgoingHandshaker::onHandshakeResponse` closed the
  pending connection on ANY error response instead of routing through `handleFailure`, so a
  `DUPLICATE_CONNECTION` rejection (the peer won the tie-break) tore the endpoint down and
  dropped its buffered messages. It now routes through `handleFailure`, which for
  `DUPLICATE_CONNECTION` marks the pending connection `replaceAsDuplicate()` — silencing it so
  the peer's subsequent connection close cannot drive or tear down the endpoint that is about
  to be handed the winning connection. The error response precedes that close on the same
  association (per-association FIFO), so the silencing is always in place in time, making the
  convergence race-free. Acceptance stress loop: 500 `--gtest_repeat` rounds of
  `Simultaneous*:ConnectionManagerIntegration*` with zero failures.

**Phase A1 — Identifiers, distance, contact lists.**
New classes: `Contact`, `ContactList`, `SortedContactList`, `RandomContactList`,
`RingContactList`, ring identifiers/distance, `getClosestNodes`, `getPeerDistance` (raw
20-byte XOR distance on `DhtAddressRaw`).
*Tests to port:* `unit/SortedContactList.test.ts`, `unit/RandomContactList.test.ts`,
`unit/getClosestNodes.test.ts`, ring-contact coverage from `benchmark/RingCorrectness.test.ts`
(port as a plain unit test, dropping the benchmark harness).
*Implemented (phase-A1 PR):* all classes ported to `modules/dht/contact/` (namespace
`streamr::dht::contact`) plus `getPeerDistance` in `modules/helpers/`. `getPeerDistance`
replicates npm `k-bucket`'s `KBucket.distance` as a double accumulation so the sort key
matches TS bit-for-bit (KBucket itself is phase A2). The contact lists are class templates
over `C`, constrained with concepts (`HasGetNodeId` for the sorted/random lists,
`HasGetPeerDescriptor` for the ring list) that enforce the required member at compile time
instead of by comment,
storing `shared_ptr<C>` and emitting the shared `ContactListEvents<C>`
(contactAdded/contactRemoved); the TS `hasEventListeners()` emit guard is dropped as a no-op
(emitting to zero listeners is already a no-op).
`SortedContactList` uses `std::lower_bound` for the lodash `sortedIndexBy` insertion.
`ringIdentifiers` computes the 120-bit ring id via `unsigned __int128` before the single
narrowing to double, matching TS's `Number(binaryToBigInt(...))` (a byte-by-byte double
accumulation would round differently); `RingContactList` uses `std::map<RingDistance, C>` for
the TS `OrderedMap`. The three unit tests port directly; the ring coverage is a from-scratch
`RingContactListTest` (the `RingCorrectness` benchmark needs `DhtNode`/`joinRing` and
ground-truth data files, all later phases) exercising per-side neighbour selection, the
per-side cap, reference/excluded-id filtering, removal and lookup. All 15 new tests green;
full `./test.sh` and `./lint.sh` green.

**Phase A2 — KBucket.**
New class: `KBucket` (port of npm `k-bucket`), with contact arbitration and events wired to
`streamr-eventemitter`.
*Tests:* new unit tests derived from the library's behavior contract (add/split/ping-eviction/
closest iteration), sized by what `PeerManager` actually uses.
*Implemented (phase-A2 PR):* `KBucket<C>` in `modules/dht/contact/` (namespace
`streamr::dht::contact`), a faithful port of the npm `k-bucket` binary-tree implementation —
`add` (descend / update / append / split / ping), `get`, `remove`, `closest`, `count`,
`toArray`, with the tree nodes held as `std::optional<vector>` contacts (nullopt = inner
node) and `unique_ptr` children. Sized to the DHT's usage: the default XOR distance
(`getPeerDistance`) and the default vector-clock arbiter are built in rather than exposed as
constructor options, and the `metadata` option and `toIterable` generator are dropped. The
contact type is constrained by a `KBucketContact` concept (`getId()` → `DhtAddressRaw`,
`getVectorClock()` → integral), replacing TS's `KBucketContact` interface; events
(added/removed/updated/ping) go through `streamr-eventemitter`. 13 unit tests cover the
behaviour contract through the public API (the tree is private): add/dedupe/move-to-end,
`added`; splitting retaining all 21 contacts; a non-splittable full bucket emitting `ping`
of the 3 longest-uncontacted and rejecting the overflow; `closest` by XOR distance (limited
and unlimited); remove/`removed`; the vector-clock arbiter (lower dropped, higher wins +
`updated`); count/toArray. Full `./test.sh` and `./lint.sh` green.

**Phase A3 — DhtNodeRpc and PeerManager.**
New classes: `DhtNodeRpcLocal`, `DhtNodeRpcRemote` (extends the existing `RpcRemote` base),
`PeerManager` (k-bucket neighbors, nearby/random/ring contact lists, unresponsive-contact
pinging, events).
*Tests to port:* `unit/PeerManager.test.ts`, `integration/DhtNodeRpcRemote.test.ts`,
`integration/DhtRpc.test.ts` (these two run over `FakeTransport` initially).

**Phase A4 — Router.**
New classes: `Router`, `RoutingSession`, `RouterRpcLocal`, `RouterRpcRemote`,
`RoutingTablesCache`, `getPreviousPeer`. (`DuplicateDetector` already exists.)
*Tests to port:* `unit/Router.test.ts`, `unit/RoutingSession.test.ts`,
`integration/RouterRpcRemote.test.ts`, `integration/RouteMessage.test.ts` (simulator).

**Phase A5 — Recursive operations.**
New classes: `RecursiveOperationManager`, `RecursiveOperationSession`,
`RecursiveOperationRpcLocal/Remote`, `RecursiveOperationSessionRpcLocal/Remote`.
*Tests to port:* `unit/RecursiveOperationManager.test.ts`,
`unit/RecursiveOperationSession.test.ts`, `integration/Find.test.ts`.

**Phase A6 — Data store.**
New classes: `LocalDataStore` (TTL map with stale/soft-delete semantics), `StoreManager`,
`StoreRpcLocal`, `StoreRpcRemote`.
*Tests to port:* `unit/LocalDataStore.test.ts`, `unit/StoreManager.test.ts`,
`unit/StoreRpcLocal.test.ts`, `integration/Store.test.ts`, `integration/StoreAndDelete.test.ts`,
`integration/StoreOnDhtWithTwoNodes.test.ts`, `integration/StoreOnDhtWithThreeNodes.test.ts`,
`integration/ReplicateData.test.ts`.

**Phase A7 — Discovery and join.**
New classes: `PeerDiscovery`, `DiscoverySession`, `RingDiscoverySession`.
*Tests to port:* `unit/DiscoverySession.test.ts`, `integration/DhtJoinPeerDiscovery.test.ts`,
`integration/MultipleEntryPointJoining.test.ts`.

**Phase A8 — DhtNode assembly and layering.**
New classes: `DhtNode` (composes PeerManager/Router/RecursiveOperationManager/StoreManager/
PeerDiscovery; **implements the existing C++ `Transport` interface** so a layer-1 `DhtNode` can
use a layer-0 `DhtNode` as its transport, sharing one `ConnectionManager` and connection locks),
`ExternalApiRpcLocal/Remote`, DhtNode options struct with TS defaults.
*Tests to port:* `integration/DhtNode.test.ts`, `integration/Mock-Layer1-Layer0.test.ts`,
`integration/Layer1-scale.test.ts`, `integration/DhtNodeExternalAPI.test.ts`,
`benchmark/KademliaCorrectness.test.ts` (as a slow correctness test, not a benchmark).

*Milestone A exit criterion:* a network of simulated C++ `DhtNode`s joins via entry points,
routes messages, and stores/fetches data — the full dht test pyramid below the connector level
is green, **except** the one test quarantined for phase AA below.

**Phase AA — Concurrent-connect convergence (deferred; do AFTER the rest of Milestone A is
merged and green).** Two multi-node join tests in `packages/streamr-dht/test/integration/
MultipleEntryPointJoiningTest.cpp` are currently quarantined (`DISABLED_`), both failing on the
same connection-establishment concurrency defect that only the threaded C++ runtime hits (TS is
single-threaded and never does):
- `DISABLED_NonEntryPointNodesCanJoin` — a fresh node joining a small already-formed
  2-entry-point network (expects 3 neighbours); fails ~1/3 of runs.
- `DISABLED_CanJoinEvenIfANodeIsOffline` — two nodes joining with one offline entry point
  (expects 1 neighbour each). The `Simulator::removeConnector` fix cleanly fast-fails the offline
  entry point, but the two *online* nodes still hit the same churn in ~20-30% of isolated runs
  (fine in a warm back-to-back loop, but each CI run is a fresh isolated process); with only 1
  expected neighbour there is no margin.

`CanJoinSimultaneously` (3 nodes joining at once, 2 neighbours each) is **reliable** (8/8 isolated)
and stays enabled — simultaneous joins connect symmetrically, so each pair is a genuine
simultaneous connect the tie-break already converges.

*Confirmed by traces:* the joining node discovers the earlier-joined peer (via an entry point's
`getClosestPeers`), adds it to its k-bucket, then drops it — its `onKBucketAdded` ping to that
peer fails with `SEND_FAILED: No connection to target, connection failed` and the handler
`removeContact`s it. In the trace two **concurrent outgoing** connects to that same not-yet-
connected peer are in flight (one from the PeerManager k-bucket ping on `pingExecutor`, one from
the DiscoverySession `getClosestPeers` query on the join executor, on different threads); the
endpoint is seen to adopt a pending connection then `detachFromPendingConnection` →
`enterDisconnectedState` (the churn), erase itself, and the racing `send()` then finds no
endpoint. `ConnectionManager::acceptNewConnection`'s offerer tie-break is built for a *simultaneous
connect* (one incoming + one outgoing) and converges both sides order-independently on the
offerer's connection, but it cannot disambiguate two **identical same-direction** duplicates —
they are symmetric.

*Two fixes were attempted and reverted (details in the session notes / auto-memory
`blockingwait-cooperative-executor`):* (a) send()-level per-peer serialization of outgoing
connects — **livelocked** under stress (a wait/retry cycle when a connection still churns; this
also showed the churn is not fully explained by "two outgoing collide", since a deduped single
outgoing should not churn yet the retry path still fired — the exact churn mechanism is not yet
pinned down); (b) rejecting a same-direction duplicate in `acceptNewConnection` — **regressed**
the test to always fail, because "first" is per-side (creation order at the initiator vs arrival
order at the acceptor), so the two nodes settle on *different* connections and both fail.

*Recommended approach for AA:* give each connection a **pair-stable id both peers can compare**
(e.g. carry the initiator's connection/attempt id through the handshake to the acceptor, or derive
one deterministically from the two node ids plus an exchanged nonce) and make the tie-break for
two same-direction duplicates deterministically keep the same one on both sides (e.g. lowest id) —
the only order-independent way for both peers to converge on the *same* duplicate. First reproduce
and fully pin the churn mechanism (the open puzzle above) before committing to a design. Then
re-enable the test and stress it (≥20 repeats) alongside the full connection integration suite.
This is connection-layer (phase-A0) work independent of the DHT logic, hence deferred to here.

### Milestone B — Direct connectivity (real sockets, NAT, WebRTC)

**Phase B1 — Connectivity checking and signed peer descriptors.**
New/extended: `connectivityChecker` (client side), `connectivityRequestHandler` (server side,
wired into `WebsocketServerConnector`), `createPeerDescriptor` +
`createPeerDescriptorSignaturePayload` (signed with the node's key via `SigningUtils`),
`DefaultConnectorFacade` extension to create the local peer descriptor from a connectivity
response, honoring `externalIp`/`websocketHost`/port-range options.
*Tests to port:* `unit/connectivityRequestHandler.test.ts`, `unit/createPeerDescriptor.test.ts`,
`unit/ConnectivityHelpers.test.ts` (if not already covered by the existing `ConnectivityTest`),
`integration/ConnectivityChecking.test.ts`, `integration/Websocket.test.ts`,
`integration/WebsocketConnectionManagement.test.ts` (adapted to the C++ endpoint design).

**Phase B2 — WebRTC connector.**
New classes: `WebrtcConnection` (libdatachannel `PeerConnection`/`DataChannel`; note TS's
node-datachannel is a binding over the same library, so semantics should match closely),
`WebrtcConnector`, `WebrtcConnectorRpcLocal/Remote` (offer/answer/ICE-candidate signaling over
the transport), `IceServer` config types, buffer thresholds; integrate into
`DefaultConnectorFacade` with `expectedConnectionType` selection (existing `Connectivity` and
`Offerer` helpers).
*Tests to port:* `unit/WebrtcConnector.test.ts`, `unit/WebrtcConnection.test.ts`,
`integration/WebrtcConnectorRpc.test.ts`, `integration/WebrtcConnectionManagement.test.ts`,
`integration/rpc-connections-over-webrtc.test.ts`.

**Phase B3 — dht end-to-end and TS interop.**
No new classes; hardening pass.
*Tests to port:* `end-to-end/Layer0.test.ts`, `end-to-end/Layer0Webrtc.test.ts`,
`end-to-end/Layer0MixedConnectionTypes.test.ts`, `end-to-end/Layer0-Layer1.test.ts`,
`end-to-end/Layer0Webrtc-Layer1.test.ts`, plus a **new TS-interop test** in the existing
`ts-integration` style: a C++ `DhtNode` joins a small TS layer-0 network (TS entry point),
appears in TS routing tables, and answers `getClosestPeers`/`ping`.

*Milestone B exit criterion:* a C++ node joins a real (local) mixed C++/TS DHT over websocket
and WebRTC.

### Milestone C — Trackerless network full node

**Phase C1 — Remaining leaf utilities and RPC-local units.**
Extend/port: `ContentDeliveryRpcLocal` to full duplex behavior (leaveStreamPartNotice paths,
inspection marking hooks), `TemporaryConnectionRpcLocal/Remote`, stream-part data-key
derivation (`streamPartIdToDataKey`).
*Tests to port:* `unit/ContentDeliveryRpcLocal.test.ts`, `unit/TemporaryConnectionRpcLocal.test.ts`,
`unit/StreamPartIDDataKey.test.ts`, `unit/NumberPair.test.ts` (if not already covered by the
existing `DuplicateMessageDetectorTest`), `integration/ContentDeliveryRpcRemote.test.ts`.

**Phase C2 — Neighbor discovery.**
New classes: `HandshakeRpcLocal`, `HandshakeRpcRemote`, `Handshaker` (parallel/serial target
selection, interleaving protocol), `NeighborFinder`, `NeighborUpdateManager`,
`NeighborUpdateRpcLocal/Remote`.
*Tests to port:* `unit/HandshakeRpcLocal.test.ts`, `unit/Handshaker.test.ts`,
`unit/NeighborFinder.test.ts`, `unit/NeighborUpdateRpcLocal.test.ts`,
`integration/HandshakeRpcRemote.test.ts`, `integration/NeighborUpdateRpcRemote.test.ts`,
`integration/Handshakes.test.ts`.

**Phase C3 — ContentDeliveryLayerNode.**
New classes: `ContentDeliveryLayerNode` (neighbors + four views, broadcast, duplicate
detection per publisher, RPC server wiring), `createContentDeliveryLayerNode` factory with TS
defaults (neighbor target 4, min propagation targets 2, etc. — take exact values from the
pinned TS source).
*Tests to port:* `unit/ContentDeliveryLayerNode.test.ts`, `unit/NodeList.test.ts` additions if
views need them, `integration/ContentDeliveryLayerNode-Layer1Node.test.ts`,
`integration/ContentDeliveryLayerNode-Layer1Node-Latencies.test.ts` (needs simulator latency
model), `integration/Propagation.test.ts`.

**Phase C4 — Entry points in the DHT.**
New classes: `PeerDescriptorStoreManager` (store/fetch stream entry points under
`SHA1(streamPartId)`), `StreamPartNetworkSplitAvoidance`, `StreamPartReconnect`.
*Tests to port:* `unit/PeerDescriptorStoreManager.test.ts`,
`unit/StreamPartNetworkSplitAvoidance.test.ts`,
`integration/stream-without-default-entrypoints.test.ts`,
`integration/streamEntryPointReplacing.test.ts`,
`integration/joining-streams-on-offline-peers.test.ts`.

**Phase C5 — ContentDeliveryManager and proxy server side.**
New/extended: `ContentDeliveryManager` (join/leave/broadcast per stream part, discovery-layer
node creation, proxy setup, `suppressOwnMessageLoopback`), extend `ProxyConnectionRpcLocal` and
the existing proxy path so a full node can **accept** proxy connections
(`acceptProxyConnections`).
*Tests to port:* `integration/ContentDeliveryManager.test.ts`,
`end-to-end/proxy-connections.test.ts`, `end-to-end/proxy-and-full-node.test.ts`.

**Phase C6 — NetworkStack and NetworkNode.**
New classes: `NetworkStack` (layer-0 `DhtNode` + `ContentDeliveryManager` composition,
lifecycle), `NetworkNode` (public API: start/stop, join/leave, broadcast, setProxies,
setStreamPartEntryPoints, message listeners, getNeighbors/getStreamParts/getPeerDescriptor),
`ExternalNetworkRpc`, `NodeInfoRpcLocal/Remote` + `NodeInfoClient`.
*Tests to port:* `unit/NetworkNode.test.ts`, `unit/ExternalNetworkRpc.test.ts`,
`integration/NetworkStack.test.ts`, `integration/NetworkNode.test.ts`,
`integration/NetworkRpc.test.ts`, `integration/NodeInfoRpc.test.ts`,
`end-to-end/external-network-rpc.test.ts`.

**Phase C7 — Inspection.**
New classes: `Inspector`, `InspectSession`. (Plumtree is deferred to milestone E — see
decision 3.3.)
*Tests to port:* `unit/InspectSession.test.ts`, `unit/Inspector.test.ts`,
`integration/Inspect.test.ts`, `end-to-end/inspect.test.ts`.

**Phase C8 — Full-node end-to-end and TS interop.**
*Tests to port:* `end-to-end/websocket-full-node-network.test.ts`,
`end-to-end/webrtc-full-node-network.test.ts`,
`end-to-end/content-delivery-layer-node-with-real-connections.test.ts`, plus a **new TS-interop
end-to-end test**: mixed network of TS and C++ full nodes on one stream part; messages published
from either side reach all nodes.

*Milestone C exit criterion:* a C++ `NetworkNode` joins a stream-part overlay directly,
publishes and subscribes, and interoperates with TS 103.8 nodes.

### Milestone D — Shared-library API and platforms

**Phase D1 — C API for the full node.**
Design and implement the shared-library surface next to the existing proxy API (keep
`proxyClient*` functions unchanged for compatibility): handle-based
`streamrNode*` API — create/delete with config (entry points, key material, port options),
start/stop, joinStreamPart/leaveStreamPart, publish, subscribe with a message callback,
setProxies (folding the proxy mode into the same node handle), error model matching the
existing `Error`/code convention. Exact naming to be reviewed in the PR.
*Tests:* C-API unit tests plus an end-to-end publish/subscribe test against a TS network,
mirroring `PublishToTsServerTest`.

**Phase D2 — Mobile constraints.**
Client-only operation (no websocket server, WebRTC + outbound websocket only) verified on
iOS/Android builds via the existing `iostest.sh`/`androidtest.sh` harnesses; confirm
libdatachannel WebRTC passes the iOS libc++ gate; document supported configurations.

### Milestone E — Deferred (explicitly out of scope for v1)

- Autocertifier client (TLS certificates for public websocket servers) and GeoIP location.
- Browser-targeted code paths.
- Benchmark suites (`SortedContactListBenchmark`, memory-leak soak tests) — port
  opportunistically when performance questions arise.
- `GapDiagnostics` sampling diagnostics.
- Plumtree (`PlumtreeManager`, `PlumtreeRpcLocal/Remote`, `PausedNeighbors`; TS tests
  `unit/PlumTreeManager.test.ts`, `unit/PlumTreeRpcLocal.test.ts`,
  `integration/PlumTreePropagation.test.ts`) — verified safely optional for interop, see
  decision 3.3; implement later as a bandwidth optimization.

---

## 5. TS test files intentionally not ported

| TS test | Reason |
|---|---|
| `unit/ManagedConnection.test.ts`, `unit/PendingConnection.test.ts`, `unit/Handshaker.test.ts` (dht), `unit/ConnectionManager.test.ts`, `unit/WebsocketClientConnector.test.ts`, `unit/WebsocketServer*.test.ts`, `unit/AddressTools.test.ts`, `unit/version.test.ts`, `unit/DuplicateDetector.test.ts`, `unit/ListeningRpcCommunicator.test.ts` | Behavior already covered by existing C++ unit tests of the redesigned connection layer; re-check coverage per file during milestone B and port any missing assertions rather than whole files |
| `unit/AutoCertifierClientFacade.test.ts`, `integration/GeoIpConnectivityChecking.test.ts`, `end-to-end/GeoIpLayer0.test.ts`, `end-to-end/RecoveryFromFailedAutoCertification.test.ts` | Autocertifier/GeoIP deferred (milestone E) |
| `unit/ProtobufMessage.test.ts` | Verifies protobuf-ts runtime behavior; C++ uses google protobuf directly |
| Browser/Karma-only tests | No C++ counterpart |

---

## 6. Risks and mitigations

1. **Threading races surfaced by ported tests.** TS logic is written assuming a single thread;
   naive translation can deadlock or race (this is where most porting time historically goes).
   Mitigation: per-class locking discipline stated up front, integration tests run under TSAN in
   CI at least for the dht package.
2. **k-bucket fidelity.** Subtle differences in eviction/arbiter behavior change routing-table
   contents and can degrade the mesh invisibly. Mitigation: dedicated unit suite in phase A2 and
   the `KademliaCorrectness` port in A8.
3. **Simulator behavioral mismatch.** The from-scratch simulator (decision 3.1, option b) is
   deliberately *not* bug-compatible with the TS one, so some ported tests may have encoded
   TS-simulator quirks rather than real protocol behavior and will need adaptation. Mitigation:
   the simulator's own unit suite pins down its guarantees (ordering, delivery, latency);
   single dispatcher thread gives determinism by construction; any test adapted because of a
   simulator-behavior difference documents that in its header comment. Residual risk is smaller
   than porting the TS implementation into a threaded runtime would be.
4. **Proto/wire drift** between pin bumps. Mitigation: the phase-0 proto-diff check script and
   the TS-interop tests at every milestone boundary.
5. **Scope creep from TS features added after the pin** (e.g. plumtree evolution). Mitigation:
   pin discipline; re-inventory the TS diff when deliberately bumping the pin.
6. **Mobile platform gates** (iOS libc++, Android). Mitigation: milestone D2 runs early builds
   of milestone B artifacts on both platforms rather than waiting for the end.

---

## 7. Definition of done

The port is feature-complete for the goal of this plan when:

1. All tests listed in milestones 0–C are green in CI (Linux + macOS), including the TS-interop
   tests at milestones B and C.
2. A C++ `NetworkNode`, configured only with the public Streamr entry points, joins the real
   Streamr network directly, subscribes to a stream part, receives messages published by TS
   nodes, and publishes messages that TS subscribers receive — over websocket and over WebRTC
   (NAT'd, client-only configuration).
3. The shared library exposes the full-node API (milestone D1) alongside the unchanged proxy
   API, and the mobile builds pass their platform test scripts.
