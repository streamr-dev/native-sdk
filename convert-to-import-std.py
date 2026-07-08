#!/usr/bin/env python3
"""
Convert the monorepo's C++ module units to `import std;`.

For each packages/**/*.cppm:
  1. remove standard-library `#include <...>` lines from the global module
     fragment (the block before `export module ...;`),
  2. add `import std;` in the module purview (right after the export line),
  3. qualify the bare C numeric aliases (`size_t` -> `std::size_t`, ...).

Why plain `import std` and not `import std.compat`: std.compat re-declares the
global replaceable `operator new`, which becomes ambiguous with libc++'s own
during over-aligned container allocation across module boundaries (observed as
"call to 'operator new' is ambiguous"). `import std` does not export those
global functions, so it is clash-free — at the cost of qualifying the bare C
aliases (std.compat would have provided them in the global namespace). The
qualification is behaviour-preserving.

Third-party includes (folly/boost/nlohmann/openssl/...), C system headers not
in the std module (unistd.h, secp256k1*, ...), and generated protobuf headers
are left untouched. C standard *macros* (SIZE_MAX, assert, ...) are NOT provided
by the module and are handled separately at their (few) use sites.

Idempotent: safe to run on the original tree or a partially-converted tree.
"""
import re
import sys
from pathlib import Path

STD_HEADERS = {
    "algorithm", "any", "array", "atomic", "barrier", "bit", "bitset",
    "charconv", "chrono", "codecvt", "compare", "complex", "concepts",
    "condition_variable", "coroutine", "deque", "exception", "execution",
    "expected", "filesystem", "format", "forward_list", "fstream",
    "functional", "future", "generator", "initializer_list", "iomanip",
    "ios", "iosfwd", "iostream", "istream", "iterator", "latch", "limits",
    "list", "locale", "map", "mdspan", "memory", "memory_resource", "mutex",
    "numbers", "numeric", "optional", "ostream", "print", "queue",
    "random", "ranges", "ratio", "regex", "scoped_allocator", "semaphore",
    "set", "shared_mutex", "source_location", "span", "spanstream", "sstream",
    "stack", "stacktrace", "stdexcept", "stdfloat", "stop_token", "streambuf",
    "string", "string_view", "strstream", "syncstream", "system_error",
    "thread", "tuple", "type_traits", "typeindex", "typeinfo",
    "unordered_map", "unordered_set", "utility", "valarray", "variant",
    "vector", "version",
    # NOTE: <new> is deliberately NOT stripped and is re-added to every unit
    # below — see ensure_new_include().
    "cassert", "cctype", "cerrno", "cfenv", "cfloat", "cinttypes", "climits",
    "clocale", "cmath", "csetjmp", "csignal", "cstdarg", "cstddef", "cstdint",
    "cstdio", "cstdlib", "cstring", "ctime", "cuchar", "cwchar", "cwctype",
}

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*<([^>]+)>')
EXPORT_MODULE_RE = re.compile(r'^\s*export\s+module\s+[\w.:]+\s*;')

C_ALIASES = (
    "size_t", "ssize_t", "ptrdiff_t", "nullptr_t",
    "int8_t", "int16_t", "int32_t", "int64_t",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "intptr_t", "uintptr_t", "intmax_t", "uintmax_t",
)
# Match comments and string/char literals FIRST (so aliases inside them are
# skipped), then a bare alias not already qualified. Only the last group is
# rewritten.
QUALIFY_RE = re.compile(
    r'(//[^\n]*)'
    r'|(/\*.*?\*/)'
    r'|("(?:\\.|[^"\\])*")'
    r"|('(?:\\.|[^'\\])*')"
    r'|(?<![\w:.>])(?<!std::)(' + "|".join(C_ALIASES) + r')\b',
    re.DOTALL)


def qualify(text):
    def repl(m):
        alias = m.group(5)
        return "std::" + alias if alias else m.group(0)
    return QUALIFY_RE.sub(repl, text)


# Module units that instantiate a std container (or other libc++ allocation)
# over a LOCALLY-DEFINED type need a textual <new> in their global module
# fragment. Reason: `import std` re-exports the global operator new
# unconditionally (libc++ std/new.inc: `export { using ::operator new; }`),
# which is ambiguous with the compiler's implicit predeclared operator new when
# libc++'s allocator is instantiated in the consuming TU ("call to 'operator
# new' is ambiguous"). A textual <new> declares ::operator new in that unit's
# own global module, merging the two candidates. This set is determined
# EMPIRICALLY (strip <new> everywhere, build --keep-going, collect the failing
# units, iterate). A future std-container-of-local-type in another unit will
# fail the same way at compile time until it is added here.
NEEDS_NEW = {
    "streamr-dht/modules/connection/Handshaker.cppm",
    "streamr-dht/modules/connection/endpoint/ConnectedEndpointState.cppm",
    "streamr-dht/modules/connection/endpoint/ConnectingEndpointState.cppm",
    "streamr-dht/modules/connection/endpoint/DisconnectedEndpointState.cppm",
    "streamr-dht/modules/connection/endpoint/Endpoint.cppm",
    "streamr-dht/modules/connection/endpoint/InitialEndpointState.cppm",
    "streamr-dht/modules/connection/simulator/Simulator.cppm",
    "streamr-dht/modules/connection/simulator/SimulatorConnection.cppm",
    "streamr-dht/modules/dht/DhtNodeRpcRemote.cppm",
    "streamr-dht/modules/dht/recursive-operation/RecursiveOperationRpcRemote.cppm",
    "streamr-dht/modules/dht/routing/RouterRpcRemote.cppm",
    "streamr-dht/modules/dht/store/StoreRpcRemote.cppm",
    "streamr-dht/modules/transport/ListeningRpcCommunicator.cppm",
    "streamr-trackerless-network/modules/logic/DuplicateMessageDetector.cppm",
    "streamr-trackerless-network/modules/logic/NodeList.cppm",
}

NEW_INCLUDE = ("#include <new> // operator new ambiguity under import std "
               "(local-type container allocation) — see convert-to-import-std.py\n")


def convert(path: Path):
    text = path.read_text()
    lines = text.splitlines(keepends=True)

    export_idx = next((i for i, ln in enumerate(lines)
                       if EXPORT_MODULE_RE.match(ln)), None)
    if export_idx is None:
        return False  # not a recognised module interface unit

    # 1. strip std includes from the global module fragment
    new_lines, stripped = [], 0
    for i, ln in enumerate(lines):
        if i < export_idx:
            m = INCLUDE_RE.match(ln)
            if m and m.group(1) in STD_HEADERS:
                stripped += 1
                continue
        new_lines.append(ln)
    body = "".join(new_lines)

    # 2. normalise the std import: std.compat -> std, or add `import std;`
    if "import std.compat;" in body:
        body = body.replace("import std.compat;", "import std;")
    uses_std = (stripped > 0
                or re.search(r'(?<![\w:])std::', body) is not None
                or any(re.search(r'(?<![\w:.>])' + a + r'\b', body)
                       for a in C_ALIASES))
    if "import std;" not in body and uses_std:
        out, inserted = [], False
        for ln in body.splitlines(keepends=True):
            out.append(ln)
            if not inserted and EXPORT_MODULE_RE.match(ln):
                out.append("\nimport std;\n")
                inserted = True
        body = "".join(out)

    # 3. for the units that need it (see NEEDS_NEW), add a textual <new> to the
    #    global module fragment to resolve the operator-new ambiguity.
    key = path.as_posix().split("packages/", 1)[-1]
    if (key in NEEDS_NEW and "import std;" in body
            and "#include <new>" not in body):
        out2, done2 = [], False
        for ln in body.splitlines(keepends=True):
            out2.append(ln)
            if not done2 and ln.rstrip("\n") == "module;":
                out2.append(NEW_INCLUDE)
                done2 = True
        body = "".join(out2)

    # 4. qualify the bare C aliases (behaviour-preserving)
    if uses_std:
        body = qualify(body)

    if body != text:
        path.write_text(body)
        return True
    return False


def main():
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("packages")
    files = sorted(root.rglob("*.cppm"))
    changed = sum(convert(f) for f in files)
    importing = sum("import std;" in f.read_text() for f in files)
    print(f"scanned {len(files)} .cppm; changed {changed}; "
          f"importing std: {importing}")


if __name__ == "__main__":
    main()
