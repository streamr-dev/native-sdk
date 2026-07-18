#!/bin/bash
# Stages the freshly built shared library next to the Python package
# sources and runs the wrapper's pytest suite against it.
set -e
cd "$(dirname "$0")/.."
# The unversioned name is a symlink to the CURRENT version (stale
# versioned dylibs may sit next to it after a version bump).
LIB=../../build/packages/streamr-libstreamrproxyclient/libstreamrproxyclient.dylib
if [ ! -e "$LIB" ]; then
    LIB=../../build/packages/streamr-libstreamrproxyclient/libstreamrproxyclient.so
fi
if [ ! -e "$LIB" ]; then
    LIB=""
fi
if [ -z "$LIB" ]; then
    echo "build the monorepo first (libstreamrproxyclient not found)" >&2
    exit 1
fi
DEST=wrappers/python/src/streamrproxyclient
if [[ "$LIB" == *.dylib ]]; then
    cp "$LIB" "$DEST/libstreamrproxyclient.dylib"
    trap 'rm -f '"$DEST"'/libstreamrproxyclient.dylib' EXIT
else
    cp "$LIB" "$DEST/libstreamrproxyclient.so"
    trap 'rm -f '"$DEST"'/libstreamrproxyclient.so' EXIT
fi
if python3 -c "import pytest" 2>/dev/null; then
    python3 -m pytest wrappers/python/tests -x -q "$@"
else
    echo "pytest not installed; running tests directly"
    python3 wrappers/python/tests/test_streamr_node.py
fi
