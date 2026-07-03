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

## Phase 1.5 — Lint stack remainder (PR pending)
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

## Phase 1.6 — CI/docs closeout
- Revisit preview runner images (macos-26 / ubuntu-26.04) once GA; consider a
  scheduled iOS build job (build-only, no signing; upload XCFramework
  artifact).
- README: per-platform C++26 feature-availability note (Homebrew libc++ vs
  iOS SDK libc++ at the deployment target vs NDK libc++), baseline-bump
  procedure.

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

## Lint/IDE survival
- During the façade stage, headers remain the fully-linted source of truth
  (`lint.sh` globs only `*.hpp/*.cpp`); `.cppm` added to clang-format only.
- Import-using test files and (post-consolidation) purview code depend on
  clangd's `--experimental-modules-support` — canary in 2.1; fallback:
  exclude import-using files from clangd-tidy until clangd modules support
  stabilizes.

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
