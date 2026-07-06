#!/bin/bash
# Verifies that this repo's .proto files are byte-identical to the pinned
# TypeScript reference implementation (streamr-dev/network). The pinned
# protos are the single source of truth for the wire format
# (trackerless-network-completion-plan.md, phase 0.1); any local edit to a
# .proto file must instead be done by bumping the pin and re-copying.
#
# Sources for the reference protos, in order of preference:
#   1. A local checkout of streamr-dev/network containing the pinned commit
#      (default: ../network relative to this repo, override with
#      STREAMR_TS_NETWORK_DIR). Read with `git show`, so the checkout does
#      not need to have the pin checked out.
#   2. raw.githubusercontent.com at the pinned commit (used in CI).

set -e

# TS reference pin: v103.8.0-rc.3
PIN=af966cf03bd2410d89e5b6499898b496aaeab810

REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
TS_DIR="${STREAMR_TS_NETWORK_DIR:-$REPO_DIR/../network}"

# local proto path (relative to this repo) -> upstream path (relative to
# the network repo root)
PROTO_MAP=(
    "packages/streamr-dht/protos/DhtRpc.proto:packages/dht/protos/DhtRpc.proto"
    "packages/streamr-dht/protos/tests.proto:packages/dht/protos/tests.proto"
    "packages/streamr-proto-rpc/protos/ProtoRpc.proto:packages/proto-rpc/protos/ProtoRpc.proto"
    "packages/streamr-trackerless-network/protos/NetworkRpc.proto:packages/trackerless-network/protos/NetworkRpc.proto"
)

fetch_reference() {
    local upstream_path="$1"
    if git -C "$TS_DIR" cat-file -e "$PIN" 2>/dev/null; then
        git -C "$TS_DIR" show "$PIN:$upstream_path"
    else
        curl --fail --silent --show-error \
            "https://raw.githubusercontent.com/streamr-dev/network/$PIN/$upstream_path"
    fi
}

failures=0
for entry in "${PROTO_MAP[@]}"; do
    local_path="${entry%%:*}"
    upstream_path="${entry#*:}"
    if ! fetch_reference "$upstream_path" |
        diff -u --label "$upstream_path@$PIN" --label "$local_path" \
            - "$REPO_DIR/$local_path"; then
        failures=$((failures + 1))
    fi
done

if [ "$failures" -ne 0 ]; then
    echo "ERROR: $failures proto file(s) differ from the pinned TS reference ($PIN)." >&2
    echo "Protos must be adopted verbatim from streamr-dev/network; see" >&2
    echo "trackerless-network-completion-plan.md phase 0.1." >&2
    exit 1
fi
echo "All protos match the pinned TS reference ($PIN)."
