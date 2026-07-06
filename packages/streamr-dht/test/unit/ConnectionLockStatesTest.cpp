#include <gtest/gtest.h>

import streamr.dht.ConnectionLockStates;
import streamr.dht.Identifiers;

using streamr::dht::DhtAddress;
using streamr::dht::connection::ConnectionLockStates; // NOLINT
using streamr::dht::connection::LockID;

TEST(ConnectionLockStates, ItCanBeConstructed) {}

// NB: boolean assertions are written as EXPECT_EQ(..., true/false)
// instead of EXPECT_TRUE/EXPECT_FALSE: clangd 22 falsely diagnoses
// member_function_call_bad_type on every EXPECT_TRUE/EXPECT_FALSE
// expansion in a translation unit that imports
// streamr.dht.ConnectionLockStates (the compiler accepts either form;
// EXPECT_EQ expands through a code path clangd resolves correctly).
TEST(ConnectionLockStates, TracksPrivateConnections) {
    ConnectionLockStates states;
    const DhtAddress nodeId{"aabbcc"};

    EXPECT_EQ(states.isPrivate(nodeId), false);

    states.addPrivate(nodeId);
    EXPECT_EQ(states.isPrivate(nodeId), true);
    EXPECT_EQ(states.getPrivateConnections().size(), 1);
    EXPECT_EQ(states.getPrivateConnections().contains(nodeId), true);

    states.removePrivate(nodeId);
    EXPECT_EQ(states.isPrivate(nodeId), false);
    EXPECT_EQ(states.getPrivateConnections().size(), 0);
}

TEST(ConnectionLockStates, PrivateConnectionsAreIndependentOfLocks) {
    ConnectionLockStates states;
    const DhtAddress nodeId{"aabbcc"};

    states.addPrivate(nodeId);
    EXPECT_EQ(states.isLocked(nodeId), false);

    states.addLocalLocked(nodeId, LockID{"lock1"});
    states.clearAllLocks(nodeId);
    EXPECT_EQ(states.isPrivate(nodeId), true);
}
