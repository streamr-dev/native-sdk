# Round-trip test of the Python StreamrNode wrapper (phase D3b): mirrors
# StreamrNodeTest.TwoNodesExchangeMessages. Needs the built shared
# library next to the package sources (hatch_build.py copies it for
# package builds; for a repo checkout run scripts/run-python-tests.sh,
# which stages the dylib and invokes pytest).
import sys
import time
import threading
import os

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from streamrproxyclient.libstreamrproxyclient import (  # noqa: E402
    LibStreamrProxyClient,
    Proxy,
    ProxyClientException,
    StreamrNode,
)

ETHEREUM_ADDRESS_A = "0x1234567890123456789012345678901234567890"
ETHEREUM_ADDRESS_B = "0x1234567890123456789012345678901234567892"
ETHEREUM_PRIVATE_KEY = (
    "1111111111111111111111111111111111111111111111111111111111111111")
STREAM_PART_ID = "0xa000000000000000000000000000000000000000#01"
ENTRY_POINT_PORT = 44471
TOPOLOGY_TIMEOUT_S = 60
MESSAGE_TIMEOUT_S = 30


def wait_until(condition, timeout_s):
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        if condition():
            return True
        time.sleep(0.2)
    return condition()


def test_invalid_ethereum_address_raises():
    with LibStreamrProxyClient() as lib:
        try:
            with StreamrNode(lib, "not-an-address"):
                raise AssertionError("expected ProxyClientException")
        except ProxyClientException as e:
            assert e.error.code == "INVALID_ETHEREUM_ADDRESS"


def test_two_nodes_exchange_messages():
    with LibStreamrProxyClient() as lib:
        with StreamrNode(lib, ETHEREUM_ADDRESS_A,
                         websocket_port=ENTRY_POINT_PORT) as node_a:
            node_a.start()
            entry_point = Proxy(
                f"ws://127.0.0.1:{ENTRY_POINT_PORT}", ETHEREUM_ADDRESS_A)
            with StreamrNode(lib, ETHEREUM_ADDRESS_B,
                             entry_points=[entry_point]) as node_b:
                node_b.start()

                received_a = []
                received_b = []
                lock = threading.Lock()

                def on_message_a(_stream_part_id, content):
                    with lock:
                        received_a.append(content)

                def on_message_b(_stream_part_id, content):
                    with lock:
                        received_b.append(content)

                sub_a = node_a.subscribe(STREAM_PART_ID, on_message_a)
                sub_b = node_b.subscribe(STREAM_PART_ID, on_message_b)

                assert wait_until(
                    lambda: node_a.neighbor_count(STREAM_PART_ID) >= 1
                    and node_b.neighbor_count(STREAM_PART_ID) >= 1,
                    TOPOLOGY_TIMEOUT_S)

                node_a.publish(STREAM_PART_ID, b"hello from A",
                               ETHEREUM_PRIVATE_KEY)
                assert wait_until(
                    lambda: b"hello from A" in received_b,
                    MESSAGE_TIMEOUT_S)

                node_b.publish(STREAM_PART_ID, b"hello from B")
                assert wait_until(
                    lambda: b"hello from B" in received_a,
                    MESSAGE_TIMEOUT_S)

                node_b.unsubscribe(sub_b)
                node_a.unsubscribe(sub_a)
                node_b.stop()
            node_a.stop()


if __name__ == "__main__":
    # Direct run without pytest (scripts/run-python-tests.sh falls back
    # to this when pytest is unavailable).
    test_invalid_ethereum_address_raises()
    print("test_invalid_ethereum_address_raises PASSED")
    test_two_nodes_exchange_messages()
    print("test_two_nodes_exchange_messages PASSED")
