// Module streamr.dht.consts
// Ported from packages/dht/src/dht/consts.ts (v103.8.0-rc.3).
module;
#include <new>


export module streamr.dht.consts;

import std;

import streamr.dht.Identifiers;

export namespace streamr::dht {

// TODO (from TS): move this to the trackerless-network package and make the
// serviceId a required parameter.
inline const ServiceID CONTROL_LAYER_NODE_SERVICE_ID{"layer0"};

} // namespace streamr::dht
