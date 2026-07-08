# Module source-location de-duplication plan

## Problem

Composing the full DHT node and the simulator connection stack in a single
translation unit (which every phase-A8 `DhtNode` integration test must do)
crashes clang:

```
fatal error: malformed or corrupted precompiled file: 'ran out of source locations'
clang frontend command failed with exit code 139
```

This is a **known, unfixed clang limitation**, not a code defect. In its
default configuration clang has an ~2 GB (2³¹) per-TU *source-location address
space*; a TU that imports many modules, each of which textually `#include`s the
same heavy headers in its global module fragment, exhausts it when clang has to
**merge the duplicated definitions** from those modules.

- LLVM #139453 — Richard Smith (clang modules maintainer):
  <https://github.com/llvm/llvm-project/issues/139453#issuecomment-2874152353>
  ("Clang requires … the preprocessed source … fit into a 2GB source location
  space. It's possible to exceed that limit if you import enough modules that
  textually `#include` a lot of headers each … The best way to handle this
  issue is to modularize as much of your dependencies as you can … import
  [a wrapper module] instead of directly `#include`ing the headers from the
  global module fragment.")
- LLVM #117541 — same error: <https://github.com/llvm/llvm-project/issues/117541>
- The 64-bit source-location patch (D97204) was **never merged**, so there is no
  compiler flag to raise the limit.

### Measured evidence (clang 22.1.8, this repo)

`#pragma clang __debug sloc_usage` on the failing TU:

| Translation unit | source-location usage |
|---|---|
| `import DhtNode;` alone | 0% |
| `import SimulatorTransport;` alone | 0% |
| `import DhtNode; import SimulatorTransport;` | **99% → crash** |
| `import DhtNode; import Simulator;` | 78% (fits) |
| `import SimulatorTransport; import Simulator; …` | 65% (fits) |

Top duplicated files in the crashing TU (`file entered N times`):

| Header | ×N | space |
|---|---|---|
| `arm_neon.h` (via **absl**, pulled by protobuf) | 48 | 153 MB |
| protobuf `port_def.inc` | 2060 | 69 MB |
| macOS SDK `Availability.h` | 1562 | 45 MB |
| **`DhtRpc.pb.h`** | 40 | 24 MB |
| libc++ `string`/`map`/`variant`/`optional`/`tuple`/… | 50–80 each | ~100 MB total |
| absl `raw_hash_set.h`, protobuf `descriptor.h` | 45 each | ~13 MB |

The single largest lever is the generated protobuf header stack
(`DhtRpc.pb.h` → protobuf runtime → absl → `arm_neon.h`), textually included in
**60** module `.cppm` files. The remainder is libc++ standard headers.

## Goal / success criterion

A TU that imports both `streamr.dht.DhtNode` and
`streamr.dht.SimulatorTransport` compiles under the source-location limit, so
the phase-A8 `DhtNode` integration tests build and pass. Re-verify after each
step with `#pragma clang __debug sloc_usage`.

The work is split into three independent steps, done **one at a time**, each
re-measured before moving on.

---

## Step 1 — Folly  ✅ already modularized (no action)

**Investigated first (per request). Finding: folly is already centralized.**
Only 8 `.cppm` files textually `#include <folly/...>`, and every one is an
intended single-owner wrapper:

- `streamr-utils/modules/CoroutineHelper.cppm` (folly coroutine headers)
- `streamr-utils/modules/ExecutorHelper.cppm` (folly executor/scheduler headers)
- `streamr-logger/modules/detail/*.cppm` ×6 (folly-logging wrappers)

Every other module reaches folly by `import streamr.utils.CoroutineHelper` /
`ExecutorHelper` / the logger modules, so the folly BMIs are shared, not
duplicated. Folly headers do **not** appear among the duplication offenders
above. **No folly work is required** — the earlier suspicion that folly was
un-modularized does not hold up against the evidence.

(If we ever want to shave more: the logger package textually includes several
folly-logging headers across 6 detail modules; consolidating those to one owner
is a minor future cleanup, not on the critical path.)

---

## Step 2 — Protobuf  ✅ DONE (solved it on its own)

**Result:** converted 57 `streamr-dht` modules from `#include "DhtRpc.pb.h"` to
`import streamr.dht.protos` (+ re-exported `google::protobuf::Any`/`Timestamp`
from the protos module, + added the std headers the pb.h had been transitively
providing: `<algorithm>`/`<chrono>`/`<exception>`/`<optional>`/`<functional>`/
`<map>`/`<atomic>`/`<ranges>`/`<string>` to ~10 modules). The `streamr-dht`
library builds clean, and the `DhtNode + SimulatorTransport` TU dropped from
**99% (crash) → 57%** (2.14 GB → 1.23 GB; ~900 MB reclaimed). This is enough
headroom that **Step 3 is not needed** for the A8 tests. The A8 integration
tests are re-enabled.

<details><summary>original plan for step 2</summary>

**Current state:** 60 `streamr-dht` module `.cppm` files textually
`#include "packages/dht/protos/DhtRpc.pb.h"` in their global module fragment;
**0** import `streamr.dht.protos`. The `streamr.dht.protos` module already exists
for exactly this purpose (its own comment: "how importers see the types without
re-parsing the 12k-line header stack") and 10+ test files already import it
successfully. `streamr-proto-rpc` has the analogous `ProtoRpc.pb.h` / protos
module.

**Change:** in each module interface unit, replace
`#include "packages/dht/protos/DhtRpc.pb.h"` (and the ProtoRpc equivalent) with
`import streamr.dht.protos;` (and `import streamr.protorpc.protos;`), and add
`using` declarations where a bare `::dht::X` was previously in scope from the
textual include. The generated `*.pb.cc` stays `#include`-based and is compiled
+ linked as today (only the *header* stops being textually duplicated across
BMIs).

**Approach (incremental, verifiable):**
1. **Spike:** convert ONE leaf module (e.g. `getPreviousPeer.cppm`) from textual
   include to `import streamr.dht.protos`; rebuild `streamr-dht`; confirm clean.
   Resolve any type that `streamr.dht.protos` does not yet re-export by adding it
   to the protos module's export list.
2. Convert the modules in the `DhtNode` + `SimulatorTransport` closure first
   (that is what the A8 tests need), rebuild, then **re-measure**
   `DhtNode + SimulatorTransport` sloc usage — expect a large drop from 99%.
3. If under the limit with headroom: convert the remaining `streamr-dht` modules
   (and `streamr-proto-rpc`) for consistency and build-speed, and re-enable
   `test/integration/MultipleEntryPointJoiningTest.cpp` (+ the rest of the A8
   suite) in `CMakeLists.txt`.
4. If still over the limit, proceed to Step 3.

**Watch for:** modules whose global module fragment genuinely needs a complete
protobuf type before `export module` (rare — the protos `using` re-exports carry
complete types); any `::dht::` enum/message not in the protos export list (add
it); ODR/linkage of generated code (unchanged — same `.pb.cc` objects linked).

**Verification:** `streamr-dht` builds; `DhtNode + SimulatorTransport` TU under
2 GB; unit + integration suites green; `lint.sh` clean.

---

</details>

## Step 3 — `import std`  ⟵ deferred (NOT needed after Step 2)

**Remaining offenders after Step 2** are libc++ standard headers
(`string`, `map`, `vector`, `variant`, `optional`, `tuple`, `unordered_map`, …)
textually `#include`d in module global fragments and duplicated 50–80× each.
The fix is to replace those global-fragment `#include <...>` with `import std;`
so the standard library comes from the single `std` module BMI.

`CMAKE_CXX_MODULE_STD` is already `ON` for the host build, so `import std;` is
available there. **However**, the Android NDK cross-build does not yet ship an
`import std` module, and this repo must keep building for `arm64-android`
(per the monorepo iOS/Android gates). So this step is **deferred until the
Android NDK supports `import std`**, or must be host-gated behind a build option
(only host TUs use `import std`; Android keeps textual includes). Revisit once
Step 2's measured result is known — Step 2 alone may bring the A8 TUs under the
limit, making Step 3 unnecessary for now.

---

## Notes

- Re-run the measurement at each checkpoint:
  `#pragma clang __debug sloc_usage` placed after the imports in a probe TU,
  compiled with the failing test's `.modmap` (all `-fmodule-file=` entries) and
  `-fsyntax-only`.
- The A8 production code (`DhtNode`, `ExternalApiRpcLocal/Remote`, the PeerManager
  descriptor getters) already compiles; this plan only unblocks the A8
  *integration tests*. `MultipleEntryPointJoiningTest.cpp` and the
  `DhtNodeTestUtils.hpp` helper are written and waiting behind this fix.
