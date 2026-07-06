# Streamr Native SDK Modernization: Toolchain Update + C++ Modules Migration

This is the working plan for the 2026 modernization effort. It is updated as
phases land; each phase ships as its own reviewed pull request.

## Context

The monorepo (8 `streamr-*` packages, ~76K LOC, vcpkg + CMake, builds for
macOS/Linux/iOS/Android) was ~2 years stale at the start of this effort:
Homebrew LLVM pinned at 17, vcpkg baseline from late 2024, clang-format/clangd
18, CMake minimum 3.22, Unix Makefiles generator. Goals: (1) update the C++
standard, compilers, tools and dependencies to the latest available, (2)
migrate the codebase to C++ named modules via CMake's modules support to cut
compile times.

**Key findings that shaped the plan (verified June 2026):**
- The repo does **not** build with Apple Clang anywhere — macOS *and* iOS
  builds use Homebrew LLVM (`cmake/homebrewClang.cmake`,
  `overlaytriplets/arm64-ios.cmake`), chainloading
  `toolchains/ios.toolchain.cmake` only for the iOS SDK/sysroot.
- **CMake (even 4.3) does not support C++ modules with AppleClang at all**;
  supported = LLVM/Clang 16+, GCC 14+, MSVC, and only Ninja/VS generators. So
  the real iOS upper limit is the **deployment target's libc++ runtime
  availability**, not Apple Clang — and staying on Homebrew LLVM is what makes
  modules possible.
- `CMAKE_CXX_STANDARD` was already 26 everywhere (nominal; Clang 17
  implemented it only partially).
- 7 of 8 packages are effectively header-only (the STATIC ones compile only
  checked-in `.pb.cc` files); all real compile cost sits in ~85 GTest TUs +
  the proxyclient TU re-parsing giant header stacks (`DhtRpc.pb.h` 12,452 LOC,
  `NetworkRpc.pb.h` 7,479 LOC, folly coro headers).

**Locked decisions:** Homebrew LLVM latest on Apple platforms; keep C++26
(+`CXX_STANDARD_REQUIRED`); `import std` as opt-in flag only (off by default);
Linux switches GCC → Clang + libc++.

**Monorepo design principles (must stay respected throughout):**
1. Each package must remain easy to release as a separate vcpkg package later —
   this is WHY the cmake helper files are duplicated per package; copies are
   kept (with `sync-cmake-files.sh` keeping them in sync), never symlinked.
2. It must remain possible to work on one package at a time, compiling and
   testing that package alone (per-package build dirs + `build.sh`/`test.sh`
   stay first-class).
3. The compiler's output must stay compatible with the iOS runtime libc++ that
   ships on devices and cannot be changed — historically this is what blocked
   newer LLVM versions. Every compiler/libc++ change gates on an iOS build +
   device test.

**Process:** one sub-phase at a time → PR → review on GitHub → merge to main →
next sub-phase branches from updated main.

**Version targets (re-verify exact patch versions at implementation time):**
brew `llvm` 22.1.x (keg-only), apt `llvm-toolchain-noble-22`, CMake floor
`3.28...4.3`, vcpkg latest tag (protobuf 25.1→33.x **requires regenerating
checked-in .pb.cc/.pb.h**, folly 2024.08→2026.02, boost 1.91, openssl 3.6,
libdatachannel 0.24), leetal/ios-cmake latest (≥4.5.0), Xcode 26.x SDK, iOS
deployment target 13.0→**17.0** (product decision, flagged; fallback 16.0),
Android NDK r28+.

---

# PART 1 — Toolchain, tools, and dependency update

## Phase 1.0 — Baseline ✅ (PR #20, merged)
- Submodule reproducibility fixed: the `wrappers/go` gitlink pointed at a
  commit no longer reachable on its remote, breaking
  `git submodule update --init --recursive` for every fresh clone (and
  aborting checkout of the remaining submodules). Re-pinned to v2.0.0.
- Stray `*~` editor backup files removed; `*~` gitignored.
- **Finding:** the old Homebrew `llvm@17` toolchain is unbuildable on current
  macOS: the bottle has `DEFAULT_SYSROOT=MacOSX14.sdk` compiled into the
  binary, and that SDK no longer exists under Xcode 26.x/CLT. `-isysroot`,
  `SDKROOT` and `CLANG_NO_DEFAULT_CONFIG` all fail to redirect the linker's
  `-syslibroot`; no non-invasive fix exists. The old-toolchain baseline was
  therefore verified on Linux CI only.
- CI: `fail-fast: false` so each platform reports its true status.

## Phase 1.1 — Build-system hygiene ✅ (PR #21, merged)
- **Ninja everywhere** (`CMAKE_GENERATOR=Ninja` exported by
  `install-prerequisities.sh`/`setenvs.sh`/CI env); all `test.sh` scripts
  moved from `cmake ..; make` to generator-agnostic `cmake --build build`
  (also fixing dropped ctest exit codes). One-time `./clean.sh` needed after
  the generator switch.
- **CMake floor `3.28...4.3`** (3.28 = first stable C++ modules support) and
  `CMAKE_CXX_STANDARD_REQUIRED ON` everywhere the standard is set.
- **`CMAKE_CXX_SCAN_FOR_MODULES OFF`** everywhere, until the modules
  migration: the 3.28 floor enabled CMP0155 (auto module-import scanning);
  under Ninja+GCC CMake injected `-fmodules-ts -fmodule-mapper
  -fdeps-format=p1689r5` into `compile_commands.json`, which gcc accepts but
  clangd rejects → lint broke. (Good omen for Part 2: with Clang the scan is a
  separate `clang-scan-deps` step and compile commands stay clean.)
- **Canonical `cmake/` dir + `sync-cmake-files.sh`**: per-package copies of
  `homebrewClang.cmake`/`monorepoPackage.cmake` stay (design principle 1) but
  are generated from canonical files; lint/CI fail if they drift. Merged the
  `streamr-logger` divergence: the canonical `monorepoPackage.cmake` appends
  dependency dirs to both `CMAKE_PREFIX_PATH` and `CMAKE_FIND_ROOT_PATH` (the
  latter is what works under cross-compiling toolchains).
- Root `CMakePresets.json` (host/ios/android × debug/release).
- CI diagnostics: `lint.sh` and `test.sh` emit GitHub `::error::` annotations
  (failing package output tail / `LastTestsFailed.log`).
- **Baseline finding:** the Linux `test` step was already red before the
  modernization — exactly the three TS-interop integration tests.

## Interim — ts-integration tests ✅ (PR #22, merged)
- `ts-integration-test`, `ts-end-to-end-test`, `ts-multiple-messages-test`
  temporarily disabled with a note: their install step builds the whole
  streamr network TS monorepo at a 2024 pin, which no longer works on current
  runners and is no longer necessary. Revisit AFTER the modernization with a
  current, slimmer TS setup. Diagnosis ruled out external-server dependence
  (all three run fully against a local `--local` subscriber on
  127.0.0.1:44211).
- **FINAL DISPOSITION (post-modernization, owner decision 2026-07):** the
  tests stay disabled in this repository and the harness will NOT be
  revived here. The streamr network TS packages have changed a lot since
  2024, and the TS interop harness (currently the
  `native-ts-integration` submodule) will be rewritten in a separate
  project. The disabled `add_test` blocks in
  `packages/streamr-trackerless-network/CMakeLists.txt` and
  `packages/streamr-libstreamrproxyclient/CMakeLists.txt` and the
  `run-ts-*`/`run-ts-integration-tests.sh` scripts remain as reference
  for that rewrite.

## Phase 1.2 — Compiler upgrades + CI image modernization ✅ (PR #23, merged)
- macOS: dead `llvm@17` → latest keg-only Homebrew `llvm` (22.x), located via
  `LLVM_PREFIX` (exported by `install-prerequisities.sh`, fallback
  `$HOMEBREW_PREFIX/opt/llvm`); hardcoded `/opt/homebrew/...` libc++ paths
  parameterized in the toolchain file and osx/ios triplets.
- Linux: gcc-14 → `clang-22` + **libc++** from `llvm-toolchain-noble-22`
  (uniform stdlib across all four platforms — one C++26 feature matrix, one
  modules implementation). New `arm64-linux` overlay triplet so the
  self-hosted runner gets the same compiler/stdlib as x64.
- **Lint stack had to follow the compiler** (pulled forward from Phase 1.5):
  clangd 18 cannot parse libc++ 22 headers, so clangd → 22 on both platforms;
  clang-format → 22 as well (versions must not diverge across platforms);
  mechanical reformat of 20 files; clang-tidy checks added/extended after v18
  suppressed with a note in `.clang-tidy` for later triage.
- CI images: validate matrix `[macos-latest, ubuntu-latest,
  linux-arm64-runner]` (macos-13/14 dropped; 26.x images are still preview so
  `-latest` = macos-15/ubuntu-24.04 and tracks GA); iOS/Android workflows on
  `macos-latest`, and their keyword gates now also match PR titles (the
  head-commit check can never fire on pull_request events). Cache keys salted
  so old-compiler caches can't poison clang-22 links. Install failures emit
  the tail of `install.sh` output + error-bearing vcpkg buildtree logs as
  annotations.
- Temporary port fixes, all removable at Phase 1.3's baseline bump:
  - `overlayports/fmt` (11.2.0 + one-line `<cstdlib>` patch): pinned fmt
    11.0.2 doesn't compile under clang-22/C++26; fmt 12 breaks folly 2024.08
    (removed `core.h` shim).
  - `CMAKE_POLICY_VERSION_MINIMUM=3.5` on Apple triplets: CMake 4 removed
    compatibility with pre-3.5 `cmake_minimum_required` still declared by old
    ports (libevent).
  - `VCPKG_OSX_SYSROOT=macosx` on osx triplets: CMake 4 no longer defaults
    `CMAKE_OSX_SYSROOT`, and the pinned vcpkg scripts compose
    `-isysroot ${CMAKE_OSX_SYSROOT}` unconditionally (empty value swallowed
    the next flag; broke openssl).
- Latent pre-existing bugs found and fixed:
  - The Linux triplets never passed `CMAKE_CXX_STANDARD=26` to ports (all
    other platforms did) — folly built at C++17, where clang has no
    coroutines, so its coro TUs were empty; only gcc's `-fcoroutines`
    coincidence made the old toolchain link. Ports now build at C++26 on
    Linux like everywhere else.
  - `overlayports/libdatachannel`'s `fix-cmakelists.patch` replaced upstream's
    `configure_file` with a REMOVE/COPY/RENAME dance that races under vcpkg's
    parallel debug/release configure (mutates the shared source tree).
    Replaced with `configure_file(... COPYONLY)`.
  - The arm64-ios triplet never preset folly's cross-compile `try_run`
    results the way arm64-android always did; added with values mirroring
    what the checks detect natively on arm64 Apple hosts.
- Verified: macOS build + 307/307 tests + lint green on LLVM 22/CMake 4.3;
  iOS cross-build green locally and in CI (first pre-merge iOS validation in
  the repo's history); XCFramework binary confirmed `platform=iOS`,
  `minos=13.0`, no references to runtime symbols newer than the deployment
  target; both Linux CI legs repeatedly fully green on clang-22 + libc++.
  Outstanding: physical-device `iostest.sh` run.
- **Known issue (separate workstream, accepted at merge):** the socket-based
  integration tests (ConnectionLockingTest, ConnectionManagerTest,
  WebsocketClientServerTest) intermittently fail or hang on shared macOS CI
  runners — a rotating cast, each also green on at least one macOS run, all
  consistently green locally and on Linux. Mitigations in `test.sh` (ctest
  `--repeat until-pass:2 --timeout 300`) keep hangs bounded and retries
  honest; the tests' timing/port assumptions need their own fix. This debt
  predates the modernization and was exposed by introducing macOS CI at all.
  **RESOLVED post-modernization:** the flakes were two library races in
  `WebsocketConnection`, not test timing/port assumptions — (1) incoming
  frames emitted as Data events before the application could register a
  listener (message silently dropped; fixed by deferring the rtc message
  callback to `startReceiving()`), and (2) a lock-order inversion between
  `mMutex` and rtc's callback mutex in `close()`/`destroy()` (teardown
  deadlock; fixed by calling into rtc outside `mMutex`). Reproduced 3/200
  resp. 2/100 locally before the fix; 0 failures in 925 stress runs
  (Debug + Release) after. **A third, rarer mechanism** surfaced on 2-core
  Linux runners (ConnectionLockingTest.CanLockConnections, PR #42 CI,
  2026-07-04, twice in a row then pass; never reproduced in 100 stress runs
  under 10x load on macOS): on the outgoing side the connector starts socket
  I/O inside `createConnection()`, but the endpoint's listeners on the
  returned `PendingConnection` are only registered afterwards
  (`addEndpoint` -> `changeToConnectingState`). If the main thread is
  descheduled in that window for longer than one localhost websocket +
  streamr handshake round-trip — realistic only on a loaded 2-core box —
  `onHandshakeCompleted()` runs first; it disarms the 15 s connect watchdog
  and then emits `Connected` on a fire-and-forget emitter with zero
  listeners. The emission is lost, no timeout remains armed, and the
  endpoint sits in the connecting state forever with the lock-request RPC
  buffered (signature: rpc FutureTimeout after 10 s, waitForCondition
  timeout, `hasConnection() == false`). Fixed by making `IPendingConnection`
  a `ReplayEventEmitter` (late listeners receive the latest
  `Connected`/`Disconnected`) and by making `emit()`'s latest-event store +
  handler snapshot atomic with respect to `on()` in the eventemitter, so a
  concurrently registered listener gets exactly one of live dispatch or
  replay.
- **Gate**: build/test green macOS + Linux, **and iOS cross-build +
  `iostest.sh` green — the compiler's output must stay compatible with the
  device's fixed libc++ runtime**.

## Phase 1.3 — vcpkg baseline bump + dependency wave ✅ (PR #24, merged)
Baseline: vcpkg tag **2026.06.24** (`builtin-baseline` now pinned in
`vcpkg.json`; the jq-based merge-dependencies.sh preserves the key — and
because the cache keys hash `vcpkg.json`, baseline bumps bust CI caches
automatically). Port versions: protobuf 6.33.4, folly 2026.02.23, boost
1.91, openssl 3.6.3, libdatachannel 0.24.5, fmt 12.2, secp256k1 0.7.1.
- **All five old overlays deleted**; the CMake-4 stopgaps
  (`CMAKE_POLICY_VERSION_MINIMUM`, `VCPKG_OSX_SYSROOT`) removed — the new
  vcpkg scripts/ports handle CMake 4 themselves. Three minimal overlays
  returned, each with a JUSTIFICATION.md:
  - **folly**: missing `<exception>` include in `result/rich_error_code.h`
    (libc++ 22 hard error), and `-DFOLLY_NO_EXCEPTION_TRACER=ON` — folly
    2026's exception tracer interposes `__cxa_throw` and now activates on
    macOS (the 2024 tracer was glibc-only), breaking
    `std::current_exception()` process-wide and aborting every coro error
    path. Found via minimal repro after 18 error-path tests aborted.
  - **secp256k1**: enable the recovery module (upstream default-off;
    SigningUtils needs `secp256k1_recovery.h`). The separate
    `secp256k1_precomputed` archive no longer exists (merged into the main
    lib in 0.7) — its find_library is now optional.
  - **usrsctp**: the two pre-2026 iOS patches (no `<net/route.h>` /
    `<ifaddrs.h>` in the iPhoneOS SDK) — verified still required.
- **Protobuf 25→33**: all checked-in generated code regenerated (protoc
  33.4); codegen plugin ported to the `absl::string_view` descriptor API;
  `proto.sh` scripts detect the protoc triplet dir and drop the removed
  `--experimental_allow_proto3_optional` flag. `DebugString()`-based test
  assertions replaced with field comparisons (6.33 prefixes a
  `goo.gle/debugproto` marker; the format was never stable).
- **folly 2026 API migration**: the seven `scheduleOn(exec)` call sites →
  `co_withExecutor(exec, task)` (deprecated, warnings-as-errors in lint).
- magic-enum 0.9.8 headers moved under `magic_enum/`; libdatachannel 0.24
  target renamed to `LibDataChannel::LibDataChannel`; `protobuf::libprotoc`
  removed from library targets (only the host codegen plugin needs it — the
  new port doesn't export it for cross triplets); `-fpermissive` removed.
- **Verified**: macOS full build + 307/307 tests + full lint; iOS
  cross-build green with XCFramework at `platform=iOS, minos=13.0` and no
  post-deployment-target runtime symbols.
- **Gate**: build/test green macOS + Linux; regenerated proto diff reviewed;
  folly overlay patch count documented (achieved: 1 patch + 1 configure
  option).

**Lesson from 1.2 (feeds the Part 2 install-flow rework):** switching target
triplets (host↔iOS↔Android) in the SAME package build dirs silently produces
mixed-platform binaries unless dirs are cleaned first (stale CMakeCache keeps
the old sysroot; the iOS toolchain never re-runs). The rework should give each
target its own build dir (e.g. `build-ios/`) or hard-fail on triplet mismatch.
Also: iOS package builds rely on a two-stage chainload (vcpkg.cmake includes
the triplet FILE as the chainload toolchain; the triplet then points
`VCPKG_CHAINLOAD_TOOLCHAIN_FILE` at ios.toolchain.cmake for ports) — fragile;
document/replace in 1.4.

## Phase 1.4 — iOS refresh (PR #25, in review)
- **Deployment target: 26.0** (product decision, 2026-07: iPhones keep
  themselves up to date; supporting old iOS runtimes is unnecessary). Set in
  the triplet and the CMake presets; verified in the artifact
  (`minos 26.0`). Xcode 26+ requirement guarded in
  `install-prerequisities.sh`.
- **SDK libc++ arrangement landed**: iOS builds compile with `-nostdinc++
  -isystem <iphoneos-sdk>/usr/include/c++/v1` (SDK path via `xcrun`) — SDK
  headers with the SDK runtime, availability-consistent with the deployment
  target. The Homebrew-libc++ `-isystem`, the
  `_LIBCPP_AVAILABILITY_HAS_INIT_PRIMARY_EXCEPTION=0` define and `-lc++abi`
  are gone.
- `toolchains/ios.toolchain.cmake` replaced with **pristine** leetal 4.5.0.
  Discovery: the previously vendored copy had been locally modified — glue
  at its top read `ENV{VCPKG_*_FLAGS}`, paired with env exports in the
  triplet; replacing the file silently broke ALL triplet flag flow (vcpkg's
  chainload replaces its own flag-applying toolchain). The glue now lives in
  a documented wrapper, `toolchains/streamr-ios.toolchain.cmake`, which
  consumes `VCPKG_C/CXX/LINKER_FLAGS` and includes the pristine upstream
  file — works for both port builds (cache vars) and package builds
  (two-stage chainload).
- **folly defines re-validated by experiment** (all five legacy globals
  dropped, then failures re-added, now scoped to the folly port block with
  documented reasons): `FOLLY_HAVE_MALLOC_USABLE_SIZE=0` (doesn't exist on
  iOS; folly's link check false-positives against SDK stubs) and
  `IS_AARCH64_ARCH=0` (the iOS toolchain reports `aarch64`, enabling folly's
  ELF-only assembly memcpy that Mach-O rejects). `FOLLY_HAVE_CLOCK_GETTIME`,
  `FOLLY_MOBILE`, `__APPLE__` stayed dropped. `-D__APPLE_USE_RFC_2292` kept
  (usrsctp; iOS 26 SDK still gates IPV6_PKTINFO behind an RFC choice).
- Verified: full iOS dependency set + packages + XCFramework green;
  artifact `platform=iOS, minos=26.0`; flags confirmed flowing in port logs
  and the package compile database.
- `iostest.sh` gained signing/tooling fixes surfaced by the device run:
  `IOS_DEVELOPMENT_TEAM` override (signs with a different team — e.g. a
  Personal Team — via command-line build settings when organization
  provisioning is unavailable; bundle IDs get a team-ID suffix because the
  originals are App IDs registered to the organization team), and
  `xcresulttool get object --legacy` (Xcode 16+ requires the flag for the
  old JSON format — result processing previously failed after a green run).
- **Gate**: `./install.sh --ios` → XCFramework ✓; `./iostest.sh --device`
  ✓ — **GoogleTest suite passed on an iPhone 12 mini running iOS 26.4.2**
  (deployment-target-26 / SDK-libc++ build, Personal Team signing);
  Android sanity via CI keyword.

## Phase 1.5 — Lint stack remainder ✅ (PR #26, merged)
- clangd/clang-format 22 already landed in Phase 1.2 (forced by libc++ 22).
- **clangd-tidy: submodule → PyPI**. The submodule pinned tag 0.2.1 (a
  single-script era); upstream 1.x is a Python package with dependencies
  (attrs/cattrs/typing-extensions), so a bare checkout is no longer
  runnable. The submodule is gone; `install-prerequisities.sh` does
  `pipx install clangd-tidy==1.1.1` (version-pinned) on both platforms and
  puts `~/.local/bin` on PATH; the 10 lint.sh call sites invoke it from
  PATH. The unused `clang-tidy` symlink alias went with it. Gained since
  0.2.1: `--line-filter` clang-tidy parity, diagnostic formatter fixes,
  `clangd-tidy-diff`.
- **All 11 post-18 check suppressions from Phase 1.2 removed — zero kept.**
  The full-monorepo sweep fired 77 findings (plus 7 more exposed by also
  dropping the nested test configs' suppressions, and 1 narrowing warning
  introduced by the ranges conversion itself), all fixed in code:
  readability-container-contains (26; includes two C++23
  `std::string::contains` substring cases), modernize-use-designated-
  initializers (22, incl. the two nested test configs' copies — also
  removed), modernize-use-ranges (8), readability-avoid-return-with-void-
  value (6, `return voidFn()` in void lambdas), bugprone-suspicious-
  stringview-data-usage (4, string_view constants passed to `getenv()` →
  now `const char*`), readability-redundant-casting (3, self-casts of
  `const DhtCallContext&`), bugprone-unused-local-non-trivial-variable
  (3 dead `debugString` debug leftovers deleted), performance-enum-size
  (2 enums → `std::uint8_t`), bugprone-optional-value-conversion (1,
  optional→value→optional round-trip in WebsocketServer), readability-
  use-std-min-max (1), modernize-use-starts-ends-with (1).
- **Gate**: `./lint.sh` green both platforms; full test suite green
  (several fixes touch runtime code paths); format at fixed point.

## Phase 1.6 — CI/docs closeout (PR pending)
- **Runner images** (checked 2026-07): `macos-26` arm64 is GA → all macOS
  legs (validate matrix, iOS, Android) moved to it — it ships Xcode 26,
  which the iOS deployment target requires (`macos-latest` still maps to
  macOS 15). `ubuntu-26.04` is still a preview image → Linux stays on
  `ubuntu-latest` (24.04); revisit when GA.
- **iOS/Android manual triggers**: validateios.yml and validateandroid.yml
  gain `workflow_dispatch` (manual run without commit-message keywords).
  No scheduled/timed runs (owner decision, 2026-07: no cron jobs in this
  repo). The iOS XCFramework is uploaded as a build artifact (30-day
  retention) on every iOS workflow run.
- **README**: Xcode 26 requirement stated in prerequisites; new
  per-platform C++26 feature-availability section (Homebrew/apt libc++ 22
  on macOS/Linux vs iOS SDK libc++ availability-gated by the deployment
  target vs NDK libc++; practical header-only-vs-runtime rule). The
  baseline-bump procedure was already documented in 1.3.
- **Gate**: CI green on the new images; iOS workflow runs via the PR
  keyword (validates the edited workflow file end to end).

---

# PART 2 — C++ named modules migration

## Architecture decisions
1. **One named module per package** (`streamr.json`, `streamr.logger`,
   `streamr.eventemitter`, `streamr.utils`, `streamr.protorpc`, `streamr.dht`,
   `streamr.trackerlessnetwork`), with **one partition per existing public
   header** (`streamr.utils:AbortController`, …) plus a **`:protos`
   partition** in each protobuf-using package wrapping the giant `.pb.h`
   files. The primary interface unit `export import`s all partitions →
   downstreams write one `import streamr.utils;`.
   `streamr-libstreamrproxyclient` exports **no module** — its C header
   `streamrproxyclient.h` stays the permanent public ABI; its `.cpp` becomes
   an importer.
2. **Façade pattern as transition scaffolding**: each partition is a `.cppm`
   that `#include`s the existing header in its **global module fragment** and
   re-exports names via `export using`:
   ```cpp
   module;
   #include "streamr-utils/AbortController.hpp"   // folly etc. arrive here, in GMF
   export module streamr.utils:AbortController;
   export namespace streamr::utils { using streamr::utils::AbortController; }
   ```
   GMF entities stay attached to the global module → **ODR-safe to mix
   `import` and `#include` consumers**. Un-migrated downstream packages keep
   `#include`-ing with zero edits; no hard-cut cascade. `include/` trees stay
   during the transition ONLY (an `import` cannot appear inside a header
   included mid-TU, so a hard leaf cut would force a big-bang across the whole
   chain).
   **COMMITTED END STATE: NO internal headers.** All internal code ends up in
   `.cppm` module units; `#include` survives only for (a) third-party
   libraries, (b) generated protobuf code (protoc emits headers —
   GMF-included, confined to `:protos` partitions), (c) the public C API
   header `streamrproxyclient.h`. Mechanism: per package K, after K's last
   dependent is module-based, a mandatory **consolidation PR** moves K's
   declarations into module purview and deletes K's `include/` tree
   (grep-enforced: nothing includes it). The façade is scaffolding that
   provably disappears. Known costs accepted: intra-package include cycles
   must become a partition DAG (worst: streamr-dht's connection/endpoint
   cluster — untangle or use coarse partitions per cyclic cluster); lint/IDE
   coverage of purview code depends on clangd's experimental modules support —
   canary early, weigh coverage loss consciously.
3. **Third-party stays `#include`** (folly/boost/protobuf/nlohmann are
   macro-heavy, not modules): always in GMFs, never re-exported except proto
   message types needed in public APIs (re-exported through `:protos`). folly
   types in exported signatures (e.g. `folly::coro::Task<T>`) are fine via
   decl-reachability. Pitfall guard: GMF includes must see identical compile
   definitions everywhere → flags stay centralized (Part 1 dedup).
4. **Single root build tree becomes the primary whole-monorepo workflow — but
   per-package standalone builds remain first-class** (design principle 2):
   each package's own `build.sh`/`test.sh` in its own build dir against
   siblings' exported configs must keep working, and packages must remain
   publishable as standalone vcpkg ports (principle 1). Cross-build-dir module
   export (`export(TARGETS … CXX_MODULES_DIRECTORY)`) exists but would make
   every downstream build dir re-scan/re-compile every upstream package's
   interfaces — multiplying exactly the cost modules remove. The root
   `CMakeLists.txt` already `add_subdirectory`s everything: modules flow
   natively, one scan, one BMI set, full Ninja parallelism. `install.sh` drops
   the per-package build loop for whole-monorepo builds (keeps a
   `--standalone <pkg>` escape hatch + per-package export configs with
   `CXX_MODULES_DIRECTORY` for the vcpkg/standalone path — **smoke-test
   `export(TARGETS …)` module support in the canary**; fallback =
   `install(TARGETS … FILE_SET CXX_MODULES)` + `install(EXPORT …)` into a
   local prefix).
5. **CMake mechanics**: new `cmake/StreamrModules.cmake` —
   `cmake_policy(SET CMP0155 NEW)`, Ninja guard, helper
   `streamr_add_module_library()`, and the **`STREAMR_IMPORT_STD` opt-in
   option** (sets `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD` + `CXX_MODULE_STD ON`;
   OFF by default). The 3 INTERFACE packages (json, eventemitter, logger)
   become **STATIC** with `target_sources(… PUBLIC FILE_SET CXX_MODULES …)`;
   their `INTERFACE` deps flip to `PUBLIC`. Reference conversion:
   `packages/streamr-utils/CMakeLists.txt`.
6. **Tests flip per package**: when a package migrates, its GTest TUs replace
   `#include "streamr-<pkg>/…"` with `import streamr.<pkg>;` (gtest itself
   stays `#include` — macros). This is where the compile-time win
   materializes: today every test TU re-parses the full folly-coro + pb.h +
   streamr header stack; afterwards it's parsed once per partition and loaded
   lazily as BMIs.

## Migration order (leaf-first, one package per PR, monorepo green after each)
| Phase | Package(s) | Why / special handling |
|---|---|---|
| 2.0 | Scaffolding | Single-tree workflow, `StreamrModules.cmake`, `bench.sh`, **record baselines** (clean wall-clock, incremental header-touch rebuild, `-ftime-trace` + ClangBuildAnalyzer top headers) on macOS/Linux/iOS |
| 2.1 | **streamr-eventemitter** (canary), streamr-json | Std-only concept-heavy single header = cleanest canary; json adds boost-pfr/nlohmann GMF test. Gates: `export(TARGETS)` modules smoke-test; clangd-tidy on an import-using test file |
| 2.2 | streamr-logger, streamr-utils | Logger: folly logging in GMF (verified: no public macros; magic-static singleton unaffected in GMF). Utils (21 partitions) = **folly::coro coroutine canary** under Clang 22 — budget for compiler-bug workarounds (per-header opt-out: keep a problem header include-only) |
| 2.3 | streamr-proto-rpc | `:protos` over `ProtoRpc.pb.h`; RpcCommunicator template stack; `extern "C"` test export still links |
| 2.4 | streamr-dht | 45 partitions mirroring subdirs + `:protos` over the DhtRpc trio; 41 test TUs flip. **Major bench checkpoint — the pb.h win shows here** |
| 2.5 | streamr-trackerless-network, streamr-libstreamrproxyclient | tn `:protos` over NetworkRpc; proxyclient imports only, C header untouched; full iOS XCFramework + Android smoke. **Final metrics** |
| 2.6 | Consolidation (MANDATORY, interleaved) | Per package, once its last dependent is module-based: move declarations into module purview, delete the package's `include/` tree (grep-enforced). End state: no internal headers anywhere; `#include` only for third-party, generated proto, and the C API header. Finalize lint posture; docs |

## Phase 2.0 — Scaffolding + baselines ✅ (PR #28, merged)
- `cmake/StreamrModules.cmake` (canonical, synced to all packages): Ninja +
  non-AppleClang guards, CMP0155 NEW, `streamr_add_module_library()`
  (STATIC + `FILE_SET CXX_MODULES` rooted at `modules/`),
  `streamr_enable_imports()` for import-using targets, and the
  `STREAMR_IMPORT_STD` opt-in (OFF; requires the CMake-version-specific
  experimental UUID to be supplied explicitly). Module scanning stays
  globally OFF — targets opt in via the helpers, so clangd's compile
  commands stay clean for non-migrated code.
- `bench.sh`: `clean` / `incremental [header]` / `trace` modes measuring the
  root single-tree build; prints host/compiler/parallelism with every run.
  `trace` uses a separate `build-bench-trace/` tree (shares
  `build/vcpkg_installed`) + ClangBuildAnalyzer if installed.

### Baseline (macOS, 2026-07-03)
Apple Silicon dev machine, 10 cores used, Homebrew clang 22.1.8, CMake 4.3,
Debug, Ninja; root single tree (108 TUs: all tests + generated proto +
proxyclient). Single runs on an idle machine.

| Metric | Baseline |
|---|---|
| Clean build | configure 8 s + build **102 s** |
| Incremental: touch `streamr-utils/StreamID.hpp` | 10 TUs, **19 s** |
| Incremental: touch `streamr-logger/SLogger.hpp` | 48 TUs, **62–70 s** |
| Compile CPU split (`-ftime-trace`) | frontend parse **859 s** vs backend codegen **61 s** → **93% parsing** |

ClangBuildAnalyzer expensive headers (cumulative parse, count):
1. `DhtRpc.pb.h` **96.0 s** (33×, avg 2.9 s) — becomes `streamr.dht:protos`
2. `gtest/gtest.h` 84.3 s (87×) — stays `#include` (macros)
3. `SLogger.hpp` **80.8 s** (47×) — becomes a `streamr.logger` partition
4. `Logger.hpp` 66.4 s (50×) — ditto
5. `ConnectionManager.hpp` 38.2 s (4×, **avg 9.5 s per inclusion**)
6. `RpcCommunicator.hpp` 34.7 s (21×)
7. `Identifiers.hpp` 31.2 s (31×)

Top template-instantiation sets (nlohmann parse ~22 s, `std::format` ~13 s,
magic_enum ~13 s) — instantiation cost that modules will NOT move (recorded
so post-migration numbers are read fairly).

The data confirms the plan's premise: >90% of compile CPU is repeated
header parsing, and the top of the expensive list is exactly the
`.pb.h`/streamr-header stack the partitions wrap.

- Linux and iOS baselines: not yet captured (macOS dev iteration is the
  primary metric). Capture the Linux numbers on the self-hosted arm64 box
  and an iOS build-only timing before the Phase 2.4 checkpoint.

## Phase 2.1 — Canary: streamr-eventemitter + streamr-json ✅ (PR #29, merged)
First real modules. Both packages carry the designed façade shape: one
`.cppm` partition per public header (header `#include`d in the global
module fragment, public names re-exported with `export using`), a primary
interface unit `export import`ing the partitions, INTERFACE→STATIC via
`streamr_add_module_library()`, package tests + example flipped to
`import streamr.<pkg>;`.
- **All gates passed on macOS:**
  - Standalone package builds produce BMIs + archives; `export(TARGETS …
    CXX_MODULES_DIRECTORY …)` generates the module-consumption info
    (smoke-test gate — no fallback needed).
  - Downstream `find_package` + `#include` consumers (streamr-utils
    standalone) build unchanged — the GMF façade is ODR-safe as designed.
  - Root tree: full build + **307/307 tests** (16 eventemitter + 57 json of
    them now run through `import`).
  - **clangd canary came out BETTER than planned**: clangd 22 lints
    import-using `.cpp` files through the CMake-generated module maps —
    no experimental flag, no lint exclusions. The planned fallback
    (excluding import-using files) was not needed.
- **Findings for the next phases:**
  - `import` does not leak transitive std includes the way textual
    inclusion did — flipped TUs must include what they use (test needed
    `<list>`/`<tuple>`; the example needed `<string>`). Expect a small
    include-adding pass with every package flip.
  - One genuine clangd-modules quirk: the *constrained*
    `std::string(std::string_view)` constructor template is not resolved
    in import-using files (plain constructors are fine; the compiler
    accepts either). Worked around in the example; watch for recurrence.
  - clangd's diagnostics in import-using files can carry module-expanded
    line numbers in secondary notes — primary locations are correct.
- Lint posture per plan: `.cppm` files get clang-format only (package
  lint.sh extended); headers remain the fully-linted source of truth.
- iOS gate via the PR's `iosbuild` keyword (module lib builds; tests are
  host-only). Android modules validation lands with its phase-2.5 gate.

## Phase 2.2 — streamr-logger + streamr-utils ✅ (PR #30, merged)
folly enters the global module fragment; utils is the folly::coro canary.
- **streamr-logger**: 4 partitions (Logger, LoggerImpl, SLogger,
  StreamrLogLevel) + primary unit. folly logging machinery in the GMF
  compiled without incident. `detail/` headers get no partitions (internal
  API); LoggerEnvTest, which tests detail machinery, keeps its detail
  `#include` alongside `import streamr.logger` — mixing is the designed
  property of the façade.
- **streamr-utils**: 21 partitions + primary. **The folly::coro coroutine
  canary passed with zero compiler workarounds** — waitForEvent,
  waitForCondition, collect, toCoroTask (coro Tasks in GMF, coroutine
  code re-exported) all build and run under Clang 22. The per-header
  opt-out budget went unused.
- **New mechanism discovered (now in `streamr_add_module_library()`)**:
  exported module targets need `target_compile_features(… PUBLIC
  cxx_std_26)`. When another build tree imports the target from its
  export file, CMake synthesizes a consumer-side BMI-compiling target and
  hard-errors if the standard level is not part of the exported usage
  requirements. (First surfaced when streamr-logger consumed
  streamr-json's export.)
- **Language rule fallout**: namespace-scope `constexpr` variables have
  internal linkage and cannot be re-exported — 7 header constants across
  both packages became `inline constexpr` (the correct C++17+ idiom for
  header constants regardless of modules).
- 20 test files + both examples flipped to `import`; the
  include-what-you-use pass stayed small (`<thread>`, `<chrono>`,
  `<string>`).
- **clangd-modules root cause identified**: the 2.1 quirks (constrained
  `string(string_view)` ctor, and now generic-lambda `std::visit`
  exhaustiveness + plain `std::string` overload failures) share one
  mechanism — clangd fails to unify std types between its own preamble
  and the types reached through module BMIs. It only bites where std
  types cross the module boundary in an import-using file. One test file
  (`toEthereumAddressOrENSNameTest.cpp` — its whole API is std types in
  Branded wrappers) needed the planned lint-exclusion fallback: first and
  only use so far; the compiler still typechecks it on every build.
  `bugprone-exception-escape` also surfaces on `main()` in import-using
  files (clangd sees deeper through BMIs) — legitimate findings, fixed
  with function-try-blocks.
- Verified: logger 63/63, utils 49/49 through import; downstream
  proto-rpc/dht standalone builds unchanged; root tree 307/307; full lint
  (one documented exclusion).
- **Honest bench note**: no incremental-rebuild improvement is expected
  yet — the heavy consumers of SLogger/utils headers (dht,
  trackerless-network) still `#include` them; the measured win arrives
  when those packages flip (2.4/2.5) and their test TUs load BMIs instead
  of re-parsing the header stack.

## Phase 2.3 — streamr-proto-rpc ✅ (PR #31, merged)
The first `:protos` partition — the pattern rehearsal for 2.4/2.5.
- 9 units: primary + `:protos` (wraps generated `ProtoRpc.pb.h`, exports
  `::protorpc::RpcMessage`/`RpcErrorType` + enumerators via `using enum`)
  + 7 partitions mirroring the public headers. The module surface is
  added to the EXISTING static library (it also compiles the generated
  `.pb.cc`) via the new `streamr_target_module_sources()` helper.
- **protobuf is not GMF-clean — overlay patch required**: any protobuf
  header in a global module fragment fails on arm64 under Clang 22
  ("no matching function for call to 'VarintParseSlowArm'"): the helper
  overloads are `static` (TU-local), and implicit template instantiation
  in module units happens in purview context where TU-local GMF entities
  are unusable. Fixed with a one-word overlay patch (`static` →
  `inline`, ODR-safe) — `overlayports/protobuf/` with JUSTIFICATION.md;
  first overlay added since the 1.3 cleanup (now 4). Found via minimal
  repro at the cheapest possible phase — dht's three big pb.h files
  would have hit the same wall in 2.4.
- **Imported-target modules are usable only by DIRECT consumers**: a
  target whose sources `import streamr.logger;` must link
  `streamr::streamr-logger` itself — BMI usability does not propagate
  through a native target's transitive linkage. (Good hygiene anyway:
  direct dependency for direct import.)
- **Export-alias rule established**: partitions re-export aliases the
  package declares for types in its API signatures (`RpcMessage`,
  `RpcErrorType`, `Any`, `Empty`); incidental convenience aliases
  (`SLogger`, `Task`) are NOT exported — importing TUs import the owning
  module instead.
- Library deps flipped INTERFACE/PRIVATE → PUBLIC (consumer-side BMI
  compilation needs the full usage requirements, including protobuf and
  magic_enum include dirs). 2 more constants → `inline constexpr`.
- The plan's "extern C test export" gate: no `extern "C"` exists in this
  package's sources (stale plan note; verified by grep).
- Verified: 26/26 tests through import (unit + integration incl. the
  generated client/server pb stubs), both examples build; downstream
  dht/tn standalone builds unchanged; root tree 307/307; full lint.

## Phase 2.4 — streamr-dht + THE BENCH CHECKPOINT ✅ (PR #32, merged)
The biggest package migrated, and the checkpoint did exactly what it was
designed for: **it caught an architecture flaw by measurement and forced a
(good) design change.**

### What the checkpoint found
The original per-header façade design (45 dht partitions, each
GMF-including its header) MULTIPLIED compile costs instead of cutting
them — every partition re-parsed the folly/protobuf stack, and the BMI
dependency chain amplified incremental cascades:

| Metric (macOS, idle) | 2.0 baseline | per-header partitions | **coarse partitions (adopted)** |
|---|---|---|---|
| Clean root build | 102 s / 108 TUs | 238 s / 197 TUs ❌ | **83 s / ~160 TUs (−19%)** ✅ |
| `SLogger.hpp` touch | 62–70 s / 48 TUs | 336 s / 117 TUs ❌ | **59 s** ✅ |
| `StreamID.hpp` touch | 19 s / 10 TUs | 54 s | 28 s (BMI-chain latency; honest regression, small in absolute terms) |

### The design change (applies to ALL packages, done in this phase)
**Coarse façade partitions**: each package exposes ONE `:all` partition
(GMF-includes every public header, exports all public names) plus
`:protos` where generated code exists. dht = `:protos` + `:all`;
proto-rpc likewise; utils/logger/json collapsed from 21/4/3 partitions to
one each. The import interface (`import streamr.<pkg>;`) is unchanged.
Per-header granularity was only ever needed for consolidation-stage code
placement — it returns then (per package, measured); at the façade stage
it is pure parse-cost multiplication. The 2.0 plan's architecture
decision #1 ("one partition per public header") is REVISED accordingly.

With the coarse layout the clean build is already **19% below baseline at
the façade stage** (test TUs load BMIs instead of parsing header stacks),
before any consolidation gains.

### Other findings
- The connection/endpoint include-cycle risk was a NON-ISSUE at the
  façade stage (partitions don't reference each other); the DAG question
  moves to consolidation, as planned.
- The 43-TU dht test flip produced exactly THREE one-line fixes — the
  fully-qualified using-decl house style matches partition exports well.
- `:protos` exports the full DhtRpc surface (35 messages, 6 enums via
  `using enum`, 9+9 generated client/server stubs); the 2.3 protobuf
  overlay patch held for the 12k-line header.
- **Host-environment hazard fixed**: `homebrewClang.cmake` injected
  `$HOMEBREW_PREFIX/include` as standard include dirs into every target;
  exported module targets baked them in, and consumer-side synthesized
  BMI compiles ordered them FIRST — a brew-installed folly/fmt silently
  shadowed the vcpkg ones. The global include dirs are removed (keg clang
  finds its own headers; all deps come from vcpkg).
- **Rule hardened**: an import-using TU must not also textually include
  sibling-package headers that reach the same third-party stacks —
  clangd's experimental modules support reports false ODR violations
  (folly) when preamble and BMI views overlap. Flipping the remaining
  sibling includes to imports fixed all cases; NO new lint exclusions.
- 6 more constants → `inline constexpr`.
- Verified: dht 81/81 + utils 49/49 (all suites re-run) through import;
  trackerless-network + proxyclient downstream builds unchanged; root
  tree 307/307; full lint green.

## Phase 2.5 — trackerless-network + proxyclient, FINAL METRICS ✅ (PR #33, merged)
The migration surface is complete: every C++ package that exports an API
now ships a module.
- **streamr-trackerless-network**: `:protos` over the NetworkRpc trio
  (22 messages + 4 enums in the GLOBAL namespace — NetworkRpc.proto
  declares no package, so the partition uses plain `export using ::X;` —
  plus 6+6 generated stubs, which the protoc plugin emits in
  `streamr::protorpc`) + coarse `:all` over the 13 public headers. All 12
  test TUs flipped to `import streamr.trackerlessnetwork` with ZERO
  fallout (first package to flip cleanly on the first build).
- **streamr-libstreamrproxyclient**: deliberately NO module. Its C header
  `streamrproxyclient.h` is the permanent public ABI; the implementation
  TU keeps textual includes until consolidation (its internal
  `LibProxyClientApi.hpp` dies there anyway, and mixing import with an
  internal header that reaches every stack would re-trigger the clangd
  false-ODR issue). Verified unchanged: 15/15 tests.
- New standing gate applied (2.4 lesson): a LOCAL RELEASE build+test of
  the migrated package (caught nothing this time — the folly/protobuf
  overlay patches hold).
- **The Android smoke gate (first Android build since the 1.3 dependency
  wave) earned its keep twice**:
  1. Upstream's 2026 folly port silently dropped Android from its
     `supports` expression → `./install.sh --android` failed outright at
     dependency install. The folly overlay re-allows Android
     (JUSTIFICATION.md updated); **folly 2026 builds fine on the NDK**.
  2. **`import streamr.json` failed on Android** — every imported name
     reported as undeclared (the `toJson` overload set etc.). Diagnosed
     AT THE TIME as NDK-clang `export using` overload bugs; resolved by
     forcing `STREAMR_MODULES_SUPPORTED=OFF` on Android (libraries built
     textually from headers, module units skipped via the STATIC +
     stub-source fallback so PUBLIC usage requirements stay identical,
     import-using test/example targets not built). The Android workflow
     also stopped running lint (host-independent; covered by
     validate.yml; import-using test files have no compile commands on
     an Android tree).
     CORRECTED in 2.6: the compiler was never the problem — the real
     cause was a `-pthread` BMI configuration mismatch (see the 2.6
     memo). Android now builds the modules on NDK r27+ (clang >= 18);
     the textual fallback remains only for older NDKs.

### FINAL FAÇADE-STAGE METRICS (vs Phase 2.0 baselines, macOS, idle)
| Metric | Baseline | Final (7 packages migrated) | Target | Verdict |
|---|---|---|---|---|
| Clean root build | 102 s | **78 s (−24%)** | −25% | ✅ effectively met (78–88 s across repeats) |
| `SLogger.hpp` touch | 62–70 s | 58 s (−6…−12%) | −40% | ❌ not yet — consolidation work |
| `StreamID.hpp` touch | 19 s | 32 s (+68%) | −40% | ❌ regression — BMI-chain latency |
| Expensive headers (ClangBuildAnalyzer) | `DhtRpc.pb.h` #1 (96 s, 33×); SLogger/Logger top-4 | `DhtRpc.pb.h` #9 (15.7 s, 6×); SLogger/Logger GONE from list; #1 = `gtest.h` (stays `#include` by design) | pb.h/folly leave the top | ✅ met |
| Total frontend parse CPU | 859 s | 723 s (−16%, incl. the module units) | — | ✅ |

**Honest reading**: the clean-build target is effectively met already at
the façade stage (test TUs load BMIs instead of re-parsing header
stacks). The incremental targets are NOT met and cannot be met by the
façade alone: while headers remain the textual source of truth inside the
`:all` GMFs, ANY header touch invalidates the package BMI and cascades
down the module DAG. Consolidation (2.6) is where the incremental wins
must come from — code moves into partitions, headers disappear, and a
one-partition edit stops invalidating whole packages. That was always the
sequencing; the numbers now quantify exactly why consolidation matters.

## Lint/IDE survival
- During the façade stage, headers remain the fully-linted source of truth
  (`lint.sh` globs only `*.hpp/*.cpp`); `.cppm` added to clang-format only.
- Import-using test files and (post-consolidation) purview code depend on
  clangd's `--experimental-modules-support` — canary in 2.1; fallback:
  exclude import-using files from clangd-tidy until clangd modules support
  stabilizes.

## Phase 2.6 — Consolidation: DECISION MEMO (owner decision required)
The planned end state (user-confirmed 2026-07): NO internal headers —
all code in `.cppm` module units, `#include` only for third-party,
generated proto, and the C API header. Consolidation is what delivers the
so-far-unmet incremental-rebuild targets (a partition edit stops
invalidating whole-package BMIs).

**The 2.5 Android blocker is RESOLVED (2.6).** The Phase 2.5 failure
(`import streamr.json` → every imported name "undeclared") was NOT an
NDK-clang capability gap. Root cause, found by reproducing the CI
failure locally and bisecting against NDK r27/r28/r29: the import-using
test TUs compile with `-pthread` (inherited from GTest's
`Threads::Threads`) while the module libraries' BMIs were compiled
without it; on Android `-pthread` toggles a language option that clang
validates when loading a module file
(`-Wmodule-file-config-mismatch` — the FIRST error in the log; the
"undeclared identifier" flood is its cascade), so the BMI silently
fails to load. The raw NDK compilers (clang 18, 19 and 21) all compile
and import the façade modules correctly when invoked by hand. Fix: the
module-library helpers in StreamrModules.cmake make `-pthread` a PUBLIC
compile option on Android, so the BMI and every importer always agree.
Android CI keeps using the runner's latest stable NDK (r29, clang 21);
`STREAMR_MODULES_SUPPORTED` keeps a version floor at the oldest
verified NDK clang (18 = r27), with the textual fallback below it.

### Preconditions for starting consolidation
1. ~~A modules-capable NDK on Android~~ **RESOLVED (2.6)** — the 2.5
   failure was the `-pthread` BMI mismatch above, not the compiler.
   Android builds the façade modules on NDK r27+ (verified locally on
   r27 and r29 for streamr-json + streamr-eventemitter, and on the
   Phase 2.6 PR's androidbuild leg for the full monorepo).
2. clangd modules support matures enough to lint purview code (with code
   in partitions there is no header fallback for the linter).
   **RE-CANARY 2026-07-04 (clangd 22.1.8) — much narrower than first
   recorded:**
   - Module interface units themselves (`clangd --check=<unit>.cppm
     --compile-commands-dir=<pkg>/build`) lint CLEAN, including the
     heavyweight folly/protobuf GMF units (`streamr.dht-all.cppm`). The
     only diagnostics are `misc-unused-using-decls` false positives on
     the `export using` re-export blocks — suppressible, and gone at
     consolidation anyway (consolidated code lives in purview, no
     re-export blocks).
   - Import-using consumers (e.g. `toJsonTest.cpp`) lint CLEAN — clangd
     loads the build tree's BMIs through the compile command's
     `-fmodule-file=` flags; no experimental flag needed.
   - **The ONE remaining failure class: preamble/BMI std-type
     unification.** When a std type crosses the module boundary in an
     API (e.g. `Branded<std::string>` = `EthereumAddress`), clangd
     treats the preamble's textual `std::string` (from `<string>`/gtest
     includes) and the BMI's `std::string` as distinct types → spurious
     "no matching constructor/function" (the still-excluded
     `toEthereumAddressOrENSNameTest.cpp`). Post-consolidation this
     class would hit any consumer passing std types to imported APIs —
     this is what still blocks consolidation.
   - Re-canary each LLVM release; the only case to retest is the
     std-unification one: `cd packages/streamr-utils && clangd
     --check=test/unit/toEthereumAddressOrENSNameTest.cpp
     --compile-commands-dir=build` — precondition met when it reports
     no type-mismatch diagnostics.
3. ~~The dht intra-package include cycles (connection/endpoint cluster)
   must become a partition DAG~~ **RESOLVED (post-2.6)** — a monorepo-wide
   analysis (include edges + forward-declaration edges, which are what
   break cycles textually but still force cyclic imports between
   partitions) found exactly ONE cycle in all seven packages: the
   6-header dht endpoint state-machine cluster. Untangled by making
   `EndpointStateInterface` a pure abstract interface that `Endpoint`
   implements (previously a concrete forwarder holding `Endpoint&` with
   its member definitions at the bottom of Endpoint.hpp). Every
   package's header graph is now a verified DAG, enforced continuously
   by `check-include-dag.py` in `lint.sh`.

### What consolidation buys (quantified at the 2.4/2.5 checkpoints)
- The −40% incremental-rebuild targets (currently: SLogger touch −6…−12%,
  StreamID touch REGRESSED +68% from BMI-chain invalidation).
- The final IWYU/ODR hygiene of the no-headers end state.

### DECISION (owner, 2026-07-04): consolidate NOW
The owner decided to start consolidation immediately, accepting the
clangd limitation: editor diagnostics may show false errors in the
affected pattern, and linting is disabled selectively (per file, with a
comment) where the clangd std-type-unification bug produces false
errors. Test files are the expected main victims; the linter keeps
running everywhere it is correct.

**Order — consumers first, libraries last (reverses the old
"leaf-first" idea, which was impossible):** a library's headers can
only be deleted once no other package's headers still textually include
them, and during the transition the downstream packages' headers do
exactly that. So consolidation walks the dependency chain from the top:
1. **C-1 streamr-libstreamrproxyclient** ✅ (this PR) — the internal
   header `LibProxyClientApi.hpp` merged into `streamrproxyclient.cpp`,
   which now imports the four sibling modules; the public C header is
   untouched. The Android/NDK version gate became a hard configure
   error (the textual fall-back no longer exists). Verified: Release
   build + 15/15 tests, standalone package build, package lint green
   (two bugprone-exception-escape suppressions — the checker cannot see
   through imported module interfaces; known pattern).
2. **C-2 streamr-trackerless-network** ✅ — **OWNER DIRECTIVE
   (2026-07-04): one partition file per former header, never one merged
   file.** Each of the 13 public headers (1,704 lines) became its own
   partition `modules/streamr.trackerlessnetwork-<Name>.cppm`
   (`export module streamr.trackerlessnetwork:<Name>;`), with only that
   header's third-party/std/generated includes in its global module
   fragment, `import :<Dep>;` for the old intra-package include edges,
   `import streamr.<pkg>;` for sibling packages, and its content in
   `export namespace` blocks (export-using of module-linkage entities
   is ill-formed — declarations are exported directly). The coarse
   `:all` partition is gone; the primary unit re-exports every
   partition. The include/ tree is deleted. Gotcha found twice: a
   partition must textually include what ITS code uses (std::operator+
   from <string>, folly::coro::blockingWait) — the former headers
   received these transitively from neighbors, and entities reached
   only through an imported module's global module fragment are not
   reliably reachable. The last textually-including test file
   (ProxyClientTsIntegrationTest.cpp) flipped to imports. Lint posture
   IMPROVED: clangd-tidy now runs on all 15 module units — full
   analysis coverage, zero suppressions. Verified: Release build, tn
   tests 12+1, proxyclient importer 15/15, standalone builds, lint
   green. **RETROFITTED in the C-3 PR to the settled architecture:**
   partitions became named sub-modules (`streamr.trackerlessnetwork.X`),
   the primary/umbrella unit was deleted, tests and the proxyclient
   import the individual sub-modules they use, and the MessageRef
   ordering operator moved from FifoMapWithTTL to the (exported) protos
   module — as a non-exported file-scope function it had module linkage
   and was unreachable from other modules instantiating the map
   templates. One more sub-module gotcha found here: a purview forward
   declaration of another module's class (NodeList forward-declared
   ContentDeliveryRpcRemote) attaches the name to the WRONG module and
   conflicts with the defining one — the import replaces it.
3. **C-3 streamr-dht** ✅ — the largest step, and where the module
   ARCHITECTURE was settled through three owner directives measured
   back to back:
   **(i) One file per former header** — all 45 public headers
   (5,013 lines) became per-header module units under `modules/` in the
   old directory tree; include/ deleted.
   **(ii) Named sub-modules, not partitions** (`streamr.dht.X`, not
   `streamr.dht:X`): partitions cannot be imported by external
   translation units, and the owner requires TESTS to import exactly
   the sub-modules they test ("tests are internal to the module").
   All 41 dht test files import their subjects directly.
   **(iii) NO umbrella module** — a `streamr.dht` unit re-exporting
   all sub-modules was built and measured, then DELETED: its compiled
   interface depends on every sub-module, so anyone importing it
   reinherits the edit-anything-rebuild-everything amplification.
   Downstream consumers (11 trackerless-network module units, its
   integration test, the proxyclient implementation) import the
   individual dht sub-modules they use — module imports now mirror the
   old header includes one to one. THE MEASUREMENTS (same mid-chain /
   leaf sub-module touch, whole-tree Release rebuild): partitions with
   a re-exporting primary ~480 s; named sub-modules behind an umbrella
   418/546 s; **no umbrella 129/135 s**. The remaining cost is the true
   dependency cone of central types plus the serial BMI chain; genuinely
   narrow edits (tests, unimported leaves) rebuild in seconds.
   Migration mechanics recorded for the remaining packages: sub-modules
   are stricter about visibility than partitions — each unit must
   DIRECTLY import what its code uses (the compiler names the missing
   module explicitly: "declaration of 'X' must be imported from module
   'M'"), and shorthand names inherited textually from neighboring
   headers need the file's own using-declaration. The include-hygiene
   gotcha from C-2 recurred (`<string>` for Branded-key containers ×16
   units, `<functional>`, `<mutex>`, `<exception>`, folly `Collect.h`).
   Incidental fixes: the dht standalone `install.sh` had hard-coded
   `-fsanitize=address` into every configure (committed debugging
   leftover, fatal once real code moved into module objects); removed.
   PR #43's race fix (ReplayEventEmitter base of IPendingConnection)
   ported into the sub-module during the rebase. One clangd-tidy
   exclusion (ConnectionLockingTest.cpp, owner-approved selective
   disabling — the analysis tool cannot resolve folly customization
   points through module interfaces; the compiler accepts the file).
   Verified: Release build, dht tests 83/83 (incl. the two new PR #43
   race tests), tn 12+1, proxyclient 15/15, standalone builds of the
   whole chain, lint green over all module units.
4. **C-4 streamr-proto-rpc** ✅ — the step that removed the LAST textual
   dependency chain in the tree: the protoc plugin's generated RPC stub
   headers (`*.client.pb.h` / `*.server.pb.h`), which `#include`d
   `RpcCommunicator.hpp` and thereby dragged the whole proto-rpc +
   logger header web into every package that uses generated stubs.
   **The plugin now emits C++ module units instead of headers**
   (`PluginCodeGenerator.hpp` rewritten): for each proto service file it
   generates `X.client.cppm` / `X.server.cppm`, named
   `<prefix>.XClient` / `<prefix>.XServer` where the consuming package
   passes its module family as a protoc plugin parameter
   (`--streamr_out=module_prefix=streamr.dht:./modules/gen`). Server
   stubs are pure interfaces (no proto-rpc dependency at all); client
   stubs `import streamr.protorpc.RpcCommunicator;`. Generated units
   land in `modules/gen/` (library packages — picked up by the existing
   recursive module glob, excluded from linting like all generated
   code) or next to the generated message code (proto-rpc tests and
   examples). The six proto-rpc public headers became named sub-modules
   under `modules/` per the settled architecture; include/ deleted; the
   plugin's own sources moved to `src/` (host tooling, not a public
   API). Two headers survive INTENTIONALLY, both generated message
   code: `*.pb.h` stays textual (`#include` in global module fragments)
   because protoc emits headers — exactly the end-state carve-out the
   consolidation decision defined.
   One duplicate-stub hazard removed: trackerless-network used to
   REGENERATE the dht stubs into its own tree (harmless as textual
   headers, an ODR violation as module-attached classes) — its proto.sh
   now generates DhtRpc message types only, and dht's own stub modules
   are the single definition. Tests and examples import the generated
   stub modules directly (`streamr.protorpc.test.*`,
   `streamr.protorpc.examples.*`), and the last `import
   streamr.protorpc;` umbrella consumers (4 dht units, 5 proto-rpc
   test/example files, 1 dht test) flipped to narrow imports.
   Verified: whole-tree build, 309/309 tests, standalone chain
   (proto-rpc → dht → trackerless-network → proxyclient), lint green
   over the module units.
5. **C-5 streamr-utils** ✅ — plain application of the settled
   architecture: the 21 public headers became named sub-modules
   (`streamr.utils.X`) under `modules/`, one per former header (git mv,
   history preserved); the coarse façade (`streamr.utils` +
   `streamr.utils-all`) and the include/ tree deleted; all 45 repo-wide
   `import streamr.utils;` consumers flipped to narrow imports; the
   package no longer exports an include directory. One 2024 leftover
   removed in passing: test/unit/SigninUtilsTest.cpp (typo-named,
   byte-identical duplicate of SigningUtilsTest.cpp, never in any CMake
   target — surfaced because clangd had been linting it with flags
   borrowed from a neighboring file).
   **IMPORTANT FINDING — a clang 22.1.8 modules miscompile, and the
   rule that avoids it.** The first (script-driven) consumer flip
   over-matched names and added spurious imports; among them,
   `import streamr.utils.waitForCondition;` in
   streamr.protorpc.RpcCommunicatorClientApi — a module unit that
   textually includes folly's coroutine Promise/Timeout headers in its
   own global module fragment and defines coroutine code. With that one
   spurious import added, ALL 17 RpcCommunicator tests hang
   deterministically: the response arrives, the promise is resolved,
   and the awaiting coroutine never resumes (every thread idle-parked;
   confirmed by stack traces). The imported BMI's global module
   fragment carries overlapping folly-coroutine declarations
   (waitForCondition's GMF also includes folly coro headers), and
   merging them with the importer's own textual copies miscompiles the
   promise-resumption path. Removing the unused import fixes it;
   bisection pinned exactly this import (collect alone is fine, the
   waitForCondition import alone hangs). CONSEQUENCE, now a hard rule
   (it was already the architecture's intent): **a module unit imports
   ONLY the modules whose names its code actually uses — never
   speculative or convenience imports.** The flip tooling now prunes
   against a curated per-module export-name list, and the compiler's
   "declaration of 'X' must be imported from module 'M'" errors are the
   ground truth for adding imports back.
   Verified: whole-tree build, 309/309 tests (with per-test timeouts),
   standalone chain (utils 49/49 → proto-rpc 26/26 → dht 83/83 →
   trackerless-network → proxyclient 15/15), lint green in all five
   touched packages (utils module units now clangd-tidy-linted).
6. **C-6 streamr-logger** ✅ — the 11 headers (4 public + 7 detail/)
   became named sub-modules (`streamr.logger.X`, tree-mirroring
   `modules/detail/` for the internals); façade and include/ tree
   deleted; all 43 repo-wide `import streamr.logger;` consumers flipped
   to narrow imports (virtually all of them just
   `streamr.logger.SLogger`), plus one proxyclient test that still
   included SLogger.hpp textually. streamr-json usage inside the logger
   units stays a TEXTUAL global-module-fragment include until C-7
   consolidates that package. Two new mechanical findings for the
   remaining steps: (i) namespace-scope `static constexpr` constants
   have internal linkage and cannot be exported — they become
   `inline constexpr` when a header moves into module purview (the
   detail color constants); (ii) a purview declaration of a C symbol
   silently gets module attachment and no longer matches the libc
   symbol at link time (`extern char** environ;` in FollyLoggerImpl —
   undefined symbol `environ@streamr.logger.FollyLoggerImpl`); such
   declarations belong in the global module fragment.
   Verified: whole-tree build, 309/309 tests (per-test timeouts),
   standalone chain (logger 63/63 → utils 49/49 → proto-rpc 26/26 →
   dht 83/83 → trackerless-network → proxyclient 15/15), lint green in
   all six touched packages (logger module units now
   clangd-tidy-linted). One more clangd-tidy exclusion added after the
   linux-arm64 CI leg failed: proto-rpc's RpcCommunicatorTest.cpp trips
   the known preamble/BMI std-type unification failure class on the
   Linux clangd build (spurious std::string-vs-std::string mismatch on
   every gtest assertion) while the macOS clangd of the same major
   version accepts it — owner-approved selective disabling, compiler
   coverage unaffected (fourth exclusion overall; revisit each clangd
   release).
7. **C-7 streamr-json** ✅ — the 3 headers (jsonConcepts, toJson,
   toString) became named sub-modules (`streamr.json.X`); façade and
   include/ tree deleted; the 4 importers (json's own tests + example)
   flipped to narrow imports, and the logger units' temporary TEXTUAL
   `streamr-json/toJson.hpp` include from C-6 flipped to
   `import streamr.json.toJson;`. First conversion with zero compile
   errors on the first whole-tree build — the accumulated rules
   (std-include pass, curated-name consumer flip, minimal imports,
   inline-constexpr, GMF-only C declarations) now cover the whole
   failure surface.
   **The same PR repairs the CI dependency caching** (owner asked for it
   to be folded in): the C-2 consolidation commit had accidentally
   REVERTED PR #41's per-platform cache keys in
   `.github/workflows/reusable/cached-install/action.yml` (a rebase
   mishap — #41 and C-2 were in flight simultaneously). Since C-2 the
   Android/iOS jobs shared one `ARM64-macOS-…` key with the host macOS
   leg; the host job saved first and owned the write-once key, so the
   cross builds restored a useless host cache and rebuilt every vcpkg
   dependency from source — degrading as the old per-platform entries
   were LRU-evicted (repo cache 11.7 GB > 10 GB cap): Android leg C-3
   48 min → C-6 139/123 min. The #41 scheme (`${ARCHFLAGS:-host}`
   platform component + vcpkg submodule SHA + restore-keys fallbacks +
   skip-save-on-exact-hit) is restored verbatim; the first post-merge
   run per platform repopulates the caches (slow once), after which
   Android returns to ~10 min and iOS to ~5 min.
8. **C-8 streamr-eventemitter** ✅ — the single header (the Phase 2.1
   canary) became the single named sub-module
   `streamr.eventemitter.EventEmitter`; façade and include/ tree
   deleted; all 23 importers flipped 1:1. Zero compile errors, zero
   test changes. bench.sh's incremental default now points at
   modules/StreamID.cppm (the header it used to touch no longer
   exists).

### CONSOLIDATION COMPLETE — final metrics and honest closure

The end state holds: **no internal headers anywhere.** `#include`
survives only for third-party libraries, generated protobuf message
code (`*.pb.h`), and the public C API header `streamrproxyclient.h` —
exactly the carve-outs the decision memo defined. Every package is
named sub-modules, one per former header, no umbrellas; consumers and
tests import precisely what they use; the compiler enforces the
dependency DAG that check-include-dag.py used to approximate.

Final bench.sh numbers (Debug root tree with all tests, same machine
class as the earlier tables; 2026-07-05):

| Metric | 2.0 baseline (headers) | 2.5 façade | **2.6 consolidated** |
|---|---|---|---|
| Clean root build | 102 s / 108 TUs | 78–88 s / ~160 TUs | **508 s / 225 TUs** |
| StreamID touch (module unit vs former header) | 19 s | 32 s | **153–173 s** |
| SLogger touch (imported by everything) | 62–70 s | 58 s | **392 s** |
| utils leaf with one importer (Ipv4Helper) | — | — | **152 s** |
| Test-file touch | — | — | **4 s** |

**Honest reading.** The consolidation delivers its structural goals
(headers gone, strict module DAG, ODR safety, narrow public surfaces)
and keeps genuinely narrow edits — tests, code outside the hot import
cones — at seconds. But wall-clock for clean builds and for edits to
widely-imported units REGRESSED versus both the header world and the
façade stage, for two compounding reasons measured here: (i) every one
of the ~100 real-code module units now parses its own folly-heavy
global module fragment (the façade stage parsed that code textually
only where included — its thin units were artificially cheap), and
(ii) an edited unit's rebuilt BMI invalidates its import cone, which
rebuilds SERIALLY along the module chains instead of Ninja-parallel as
textual includers used to. Where the cone is wide (SLogger), the edit
pays for the world. The remaining lever is NOT architectural — it is
caching: recompiling only what actually changed, which the module
graph now makes precise. That is the next (owner-directed) step: CI
build-directory caching, then a fresh measurement round against these
numbers.

One package per PR held throughout; headers were deleted per-package —
the compiler itself enforced that nothing still includes them.

### Third-party wrapper modules (owner-directed follow-up to the final metrics)

The census behind the 508 s clean build: the folly COROUTINE header
stack was parsed textually in ~28 module-unit global module fragments
plus ~21 test files — by far the dominant repeated parse. Everything
else was already effectively single-owner (rtc/rtc.hpp ×4 dht units;
folly logging ~6 units confined to streamr-logger; boost-uuid /
cryptopp+secp256k1 / nlohmann+boost-pfr / magic_enum each confined to
1–4 units that already act as their wrapper).

Fix (functional names per owner, living in streamr-utils):
**streamr.utils.CoroutineHelper** is now THE single owner of the folly
coroutine headers, and **streamr.utils.ExecutorHelper** of the
executor/scheduler headers. Names are re-exported in their ORIGINAL
namespaces (folly::coro::Task stays folly::coro::Task), so use sites
kept their spelling — with three exceptions: folly's blockingWait /
co_withExecutor / co_withCancellation are customization-point objects
that cannot be re-exported by using-declaration (internal linkage or
conflicting redeclarations), so they are exported as perfect-forwarding
shims spelled streamr::utils::X. The protoc plugin emits
`import streamr.utils.CoroutineHelper;` in generated stubs. Every
former textual folly-coro include was swept in the same change (the
C-5 rule: overlapping folly-coro GMFs are a miscompile risk — this
change REDUCES that surface to one owning unit; all 309 tests pass
with per-test timeouts, RpcCommunicator included).

Migration rule discovered: `std::coroutine_traits` must be VISIBLE in
every translation unit that defines OR INSTANTIATES a coroutine — it
cannot arrive through an imported BMI, so every CoroutineHelper
importer textually includes the (tiny) `<coroutine>` header, tagged
`// IWYU pragma: keep` because clangd's include-cleaner cannot see the
coroutine-transformation requirement. One more clangd-tidy exclusion of
the known std-type-unification class (dht PendingConnectionTest.cpp,
fifth overall).

**Measured effect (same Debug tree as the final-metrics table):**

| Metric | consolidated (C-8) | + wrapper modules |
|---|---|---|
| Clean root build | 508 s | **315 s (−38%)** |
| StreamID touch | 153–173 s | **94 s (−39…−46%)** |
| SLogger touch | 392 s | **326 s (−17%)** |

The SLogger case remains cone-bound: every importer still rebuilds,
just cheaper per unit. Shrinking that cone needs the next lever
(implementation units + reduced BMIs — candidate follow-up after the
CI caching step). rtc/rtc.hpp (4 dht units) is the remaining wrap
candidate if more clean-build reduction is wanted.

**CI incident during this PR (recorded for the caching step):** the
first arm64-ios vcpkg cache entry saved after the C-7 key repair
RESTORED CORRUPTLY on every later run — boost headers came back empty,
producing "use of undeclared identifier 'boost'" in Uuid.cppm while the
identical compile (same clang 22.1.8, same iPhoneOS 26.5 SDK) passed
locally. actions/cache treats a partial extraction as a warning, not an
error, and vcpkg's reconciliation trusts its status database without
checking file contents, so the damage persisted across runs. Entries
are immutable under their key; the fix was a generation bump (-v2- in
the key names) forcing one clean rebuild+save per platform. Two
diagnostics hardenings came out of it: a second CI annotation carrying
only compiler-error context (GitHub caps annotations at 4096 chars and
one module-compile command line can eat that alone), and this record.
The repository cache being over the 10 GB limit (continuous LRU
eviction) raises the odds of interrupted saves — relevant when the CI
build-directory caching step adds more entries.

### `import std` verdict (investigated 2026-07-04, child session)
Owner-requested experiment on the consolidated trackerless-network
partition, with Android as the gate. Findings (full detail and a
working conversion template on the `experiment/import-std` branch, file
`import-std-findings.md` — NOT for merging):
- Host (Homebrew LLVM 22.1.8): works, including the mixed mode the
  codebase needs (third-party textual includes + `import std;` purview).
  The existing `STREAMR_IMPORT_STD` scaffold worked unmodified; one
  toolchain packaging defect requires passing
  `-DCMAKE_CXX_STDLIB_MODULES_JSON=$LLVM_PREFIX/lib/c++/libc++.modules.json`.
- **Android gate: FAILS for practical purposes.** NDK r29 is the first
  NDK to ship the std module sources, but they do not compile against
  Bionic as delivered (internal-linkage `ctype` inlines); making them
  build requires overriding the undocumented `__BIONIC_CTYPE_INLINE`
  macro, disabling `_FORTIFY_SOURCE` hardening, and hand-patching
  CMake's cross-compilation stdlib detection. Not defensible.
- **Decision: do not adopt (not even host-only — it would fork the
  source style).** Re-test each NDK release with the one-line probe in
  the findings file; adopt when the NDK's sources compile cleanly with
  fortify enabled AND CMake's detection passes the toolchain target.
- **NDK r30 beta 1 retest (owner-requested, 2026-07-04): still FAILS —
  nothing gained by switching.** The blockers are in Bionic itself
  (internal-linkage ctype functions AND the `_FORTIFY_SOURCE` stdio
  wrappers, tested separately), textually unchanged in the pre-release.
  One improvement found during the retest: CMake's broken
  cross-compilation standard-library probe can be bypassed CLEANLY by
  adding the Android `--target` to `CMAKE_CXX_FLAGS` (replaces the
  indefensible hand-patching of generated files). The full minimal
  adoption recipe for a future fixed NDK is recorded in
  `import-std-findings.md` on the `experiment/import-std` branch; the
  probe must include `-D_FORTIFY_SOURCE=2` or the second failure stays
  hidden.

### After the consolidation: CI speed (owner-approved, 2026-07-04)
Even with the per-platform dependency caches fixed (Android ~30 → 9.6
min, iOS ~35 → 4.6 min), each CI job still compiles the monorepo from
scratch and runs the tests serially. Planned follow-up work, in payoff
order — scheduled AFTER C-8 because every consolidation step changes
the build structure the caches would key on:
1. **Stop compiling the monorepo twice per job.** `install.sh` builds
   every package standalone AND then the whole root tree. The
   standalone-package check validates packaging structure, which does
   not differ per platform — run it on ONE platform (macOS) only.
2. **Cache the build directories per platform** (with restore-keys
   fallbacks, like the dependency caches). Ninja then recompiles only
   what a pull request touches — and the one-partition-per-header
   structure is exactly what keeps the rebuild cascade small: editing
   one partition rebuilds that partition, its importers and the links,
   not whole packages. Watch the shared 10 GB actions-cache budget;
   if build trees + five dependency caches start evicting each other,
   move the dependency cache to a GitHub Packages NuGet feed
   (documented option from the cache investigation, free for public
   repositories) to reclaim the budget.
3. **ccache as a complement** if build-dir caching proves fragile:
   caches ordinary TUs (all test files — the bulk) but not module
   BMI compiles (tooling support still poor).
4. **Randomize test ports, then run ctest in parallel** — tests
   currently use fixed ports and must run serially.

**IMPLEMENTED (items 1 + 2, after C-8 and the wrapper modules, as
planned):**
- `install.sh --no-standalone` skips the per-package standalone builds;
  ALL validate legs pass it (via INSTALL_EXTRA_FLAGS in validate.yml)
  and lint against the ROOT tree's compile database — every package
  lint.sh falls back to `../../build` when its standalone database is
  absent. The packaging-structure check is carried by the iOS job,
  which builds every package standalone by construction (the
  XCFramework is assembled from the per-package outputs;
  `--no-standalone --ios` is rejected). Owner decision 2026-07-06: the
  macOS leg originally kept the loop as the designated check; that
  duplication was dropped since the iOS job already covers it.
- The cached-install action now caches the ROOT build tree per platform
  (key: platform + vcpkg SHA + commit; prefix restore-keys), excluding
  vcpkg_installed (own cache) and the per-package trees (8 × 0.5–1.5 GB
  would blow the shared 10 GB budget). SAVED ONLY FROM MAIN-BRANCH RUNS
  — pull requests consume, merges publish; a broken PR tree can never
  poison the cache (the corrupted arm64-ios entry lesson). Because
  Ninja compares source mtimes against the recorded build state, and a
  fresh checkout stamps everything with checkout time, the action first
  resets every tracked file's mtime to its last-change commit time
  (single git-log pass; needs the workflows' fetch-depth: 0) — so a
  restored tree rebuilds exactly the pull request's touched cone.
- Item 3 (ccache) remains a fallback if this proves fragile; item 4
  (port randomization + parallel ctest) remains open.
- **MEASURED (PR #54, 2026-07-06).** All legs green:

  | Leg | before this work | measured | state |
  |---|---|---|---|
  | ubuntu | 45–52 min | **5 min** | warm build tree |
  | linux-arm64 | 44–45 min | **11 min** | warm build tree |
  | macOS | 103–115 min | **36 min** | COLD, no standalone loop |
  | iOS | 68–97 min | 73 min | warm tree + #52 proto-drift rebuild; keeps the standalone loop (XCFramework) |
  | Android | 91–123 min | 70 min | cold (its tree fell to cache eviction); improves after main saves |

  The ubuntu warm path is the headline: five minutes for checkout,
  dependency restore, build-tree restore, drift rebuild, lint and
  tests. macOS improves further once a main run saves its tree, and
  Android once its tree survives in the cache budget. Follow-ups noted:
  the shared 10 GB actions-cache budget is now the binding constraint
  (three build trees ≈ 6.4 GB + two dependency-cache generations —
  the plan's NuGet-feed option for dependency caches is the relief
  valve), and item 4 (test-port randomization + parallel ctest) remains
  open.

### Interim posture (adopted now)
The façade stage is COMPLETE and delivers: uniform `import streamr.<pkg>`
consumption, −24% clean builds, 250+ tests through import, and module
infrastructure exercised on macOS/Linux/iOS/Android. Headers remain the
linted source of truth. With the Android blocker resolved (2.6) and the
header graph a verified DAG (post-2.6), consolidation is gated only by
precondition 2: clangd lint coverage of purview code. This is a stable
resting point.

## Success metrics (measured macOS + Linux, end of Phase 2.5)
- ≥25% clean-build wall-clock reduction (dev build with tests).
- ≥40% reduction on incremental rebuild after touching a mid-stack header
  (e.g. `streamr-utils/StreamID.hpp`).
- ClangBuildAnalyzer: `*.pb.h` and folly/coro headers leave the top of the
  expensive-headers list.
- Honest expectations: library targets gain ~120 module TUs each parsing GMF
  includes + scan overhead → clean-build CPU may rise while wall clock falls
  (test TUs dominate); template instantiation cost does not move; iOS (tests
  off) ≈ neutral — acceptable, the goal is dev/CI iteration speed.

## Top risks
| Risk | Mitigation |
|---|---|
| folly 2026.02 breakage (esp. Linux+libc++, iOS) | `builtin-baseline` + `overrides` pinning; overlay rebuilt patch-by-patch with evidence |
| protobuf 25→33 regen + abseil in XCFramework | Codegen plugin is 2 files; regen mechanical; verify symbols in iOS test app |
| iOS SDK-libc++ switch surfaces availability/link errors | Staged fallback: SDK headers @ DT 18 for the one macro → last resort re-add the `-D` define (never a folly patch) |
| Clang 22 modules bugs (folly::coro across module boundary, concept re-export) | Leaf-first canaries; façade allows per-header opt-out; file upstream bugs |
| `export(TARGETS)` build-tree module export unsupported | Canary gate; fallback to install-based export; single-tree path unaffected |
| clangd can't parse import-using code | Experimental flag; else lint exclusion during transition |
| ODR divergence from inconsistent GMF compile defs | Single build tree + centralized flags; never per-target folly/glog config macros |
| BMI disk bloat (folly GMFs × ~120 units); ccache handles BMIs poorly | Accept/monitor; rely on Ninja incrementality; note in CI docs |
| vcpkg standalone "monorepo"-feature builds of packages | Verify the path is actually exercised; keep export configs working |

## Verification (every phase gate)
- macOS arm64: `./clean.sh && source install-prerequisities.sh && ./install.sh && ./test.sh && ./lint.sh`
- Linux (clang-22/libc++): same. iOS: `./install.sh --ios` + `./iostest.sh` +
  XCFramework artifact check. Android: `./install.sh --android` smoke
  (Phases 1.4, 2.5).
- Part 2: `bench.sh` numbers (clean + incremental + ClangBuildAnalyzer)
  recorded in each PR description; CI green.
