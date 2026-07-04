# `import std;` feasibility experiment (branch `experiment/import-std`)

Date: 2026-07-04. Toolchains: Homebrew LLVM 22.1.8 (macOS host), Android NDK
r29 (clang 21, the version CI uses), NDK r27.1 / r28.0 for comparison,
CMake 4.3.4, Ninja.

Goal: can the consolidated module partitions (one `.cppm` per former header,
e.g. `packages/streamr-trackerless-network/modules/logic/*.cppm`) replace
their textual standard-library includes with `import std;`? The Android NDK
build is the decisive gate, since it is the platform most likely to lack
standard-library-module support.

**Bottom line: do not adopt yet.** Host support is real and works end to end
(this branch builds and its tests pass with the flag on). Android — the
decisive gate — also builds all the way to aarch64 binaries, but only behind
four workarounds, two of which are disqualifying for production: a global
override of a Bionic-internal macro, and disabling `_FORTIFY_SOURCE`. On top
of that, CMake 4.3.4 cannot detect the NDK's standard library at all (its
probe lacks the `--target` flag), so the Android configure needs hand-editing
of generated files. Details and the exact evidence below. The experiment is
preserved on this branch: one partition (`DuplicateMessageDetector.cppm`)
converted, and the build opt-in works via the pre-existing
`STREAMR_IMPORT_STD` flag.

**Warning to anyone checking out this branch:** a plain `./install.sh` (flag
off) can no longer build `streamr-trackerless-network`, because the converted
partition unconditionally says `import std;`. This is inherent — a module
purview cannot carry both forms — and is exactly why a half-adopted state is
not shippable. Do not merge this branch.

## Stage 1 — raw host feasibility (no CMake): PASSED

Homebrew LLVM 22.1.8 ships the standard-library module sources:

- `/opt/homebrew/opt/llvm/share/libc++/v1/std.cppm` (and `std.compat.cppm`,
  plus the `std/` and `std.compat/` export-declaration subdirectories),
- the manifest `/opt/homebrew/opt/llvm/lib/c++/libc++.modules.json`
  (real path `/opt/homebrew/Cellar/llvm/22.1.8/lib/c++/`).

Hand-compilation, all with `clang++ -std=c++26`:

1. `clang++ -std=c++26 --precompile -Wno-reserved-module-identifier -o std.pcm
   $LLVM_PREFIX/share/libc++/v1/std.cppm` → 34.4 MB `std.pcm`, no errors.
2. A named module `greeter` with `import std;` in its purview, plus a
   consumer translation unit doing `import std; import greeter;` — compiles,
   links, runs (`std::println` output correct).
3. **The mixing test** (the shape every consolidated partition would have):
   a module whose *global module fragment* textually includes
   `<folly/experimental/coro/Task.h>`, `<folly/experimental/coro/BlockingWait.h>`
   and `<boost/algorithm/hex.hpp>` (these are the exact third-party headers the
   trackerless-network partitions include today; they internally include large
   parts of the standard library textually) while the *purview* does
   `import std;` and uses `std::vector`, `std::optional`, `std::map`,
   `std::chrono` interleaved with folly coroutines. Compiles (52.7 MB BMI),
   links against vcpkg-built folly, runs correctly.

Caveats found:

- Clang's driver does **not** find the manifest itself:
  `clang++ -print-library-module-manifest-path` prints `<NOT PRESENT>` and
  `clang++ -print-file-name=libc++.modules.json` returns the name unresolved,
  because Homebrew installs it under `lib/c++/` which is not on the driver's
  search path. This breaks CMake's automatic discovery (see Stage 3).

## Stage 2 — THE ANDROID GATE (raw, no CMake): CONDITIONAL PASS

**NDK r29 is the first NDK that ships the standard-library module sources.**

- NDK r29 (`toolchains/llvm/prebuilt/darwin-x86_64/`): has
  `share/libc++/v1/std.cppm`, `share/libc++/v1/std.compat.cppm` and
  `lib/libc++.modules.json`. The driver even resolves
  `-print-file-name=libc++.modules.json` correctly (unlike Homebrew clang).
  The NDK changelog does not mention any of this; it is undocumented.
- NDK r27.1.12297006 and r28.0.12674087: **nothing** — no `std.cppm`, no
  manifest anywhere in the toolchain or sysroot. `import std` is impossible
  there (a from-scratch libc++ build would be the only route).

But compiling the std module out of the box **fails** against Bionic:

```
$NDK/bin/clang++ --target=aarch64-linux-android24 -std=c++26 --precompile \
    -Wno-reserved-module-identifier -o std.pcm $NDK/share/libc++/v1/std.cppm

share/libc++/v1/std/ctype.inc / std/locale.inc:
error: using declaration referring to 'isblank' with internal linkage cannot be exported
error: using declaration referring to 'iscntrl' with internal linkage cannot be exported
error: using declaration referring to 'isdigit' with internal linkage cannot be exported
... (20 errors, one per ctype function)
```

Root cause: Bionic's `ctype.h` defines the ctype functions as
`__BIONIC_CTYPE_INLINE int isdigit(int) {...}` where
`#define __BIONIC_CTYPE_INLINE static __inline` — internal linkage, which a
module cannot re-export (`export using std::isdigit;` in the std module's
`std/ctype.inc`). This is a Bionic/libc++-module incompatibility that Google
has evidently not yet addressed (the module sources ship, but were never
compile-tested against Bionic).

Workaround (works, but unsupported): Bionic guards the macro with
`#if !defined(__BIONIC_CTYPE_INLINE)`, so predefining it with
`-D__BIONIC_CTYPE_INLINE=__inline` gives the functions external linkage
(plain C++ `inline`; real symbols also exist in `libc.so`). With that define:

- the std module precompiles cleanly (31.2 MB BMI),
- the `import std` consumer + named module links into a genuine
  `ELF 64-bit LSB pie executable, ARM aarch64` against the NDK libc++,
- the folly/boost mixing test (same sources as Stage 1, folly headers built
  for arm64-android) precompiles and compiles to an object file cleanly.

**The workaround must be applied to every translation unit.** Probe: a TU that
textually includes `<cctype>` (getting Bionic's `static inline` functions)
while importing the std module built with the define fails with
`error: call to 'isdigit' is ambiguous` — the TU sees both its own
internal-linkage declaration and the module's external-linkage one. Compiled
with the define everywhere, it is clean. So on Android the define has to be a
global compile flag for the whole build (and ideally for the vcpkg-built
dependencies too, to keep one consistent declaration of libc), which is an
unsupported configuration of Bionic that Google could break in any NDK update.

## Stage 3 — CMake wiring on the real package

CMake 4.3.4's experimental UUID (extracted from the `cmake` binary, adjacent
to the `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD` string; matches upstream
`Help/dev/experimental.rst` for the 4.3 series):

```
CMAKE_EXPERIMENTAL_CXX_IMPORT_STD = 451f2fe2-a8a2-47c3-bc32-94786d8fc91b
```

Converted partition:
`packages/streamr-trackerless-network/modules/logic/DuplicateMessageDetector.cppm`
— global-module-fragment std includes removed, `import std;` added to the
purview. Two source changes were forced by the conversion: `::int64_t` and
`::size_t` had to become `std::int64_t` / `std::size_t`, because `import std;`
does not provide the global-namespace C aliases (only `import std.compat;`
does), and the unqualified names had only been leaking in through the textual
includes.

### Host (arm64-osx): PASSED

- CMake's automatic manifest discovery **fails** with Homebrew LLVM: the
  generate step aborts with `Failed to load C++ standard library modules
  metadata from "libc++.modules.json": File not found`, because CMake asks the
  compiler via `-print-file-name=libc++.modules.json` and Homebrew's clang
  cannot resolve it (see Stage 1 caveat). Fix — pass the path explicitly **on
  the first configure**:

  ```
  cmake -DCMAKE_BUILD_TYPE=Debug \
        -DSTREAMR_IMPORT_STD=ON \
        -DCMAKE_EXPERIMENTAL_CXX_IMPORT_STD=451f2fe2-a8a2-47c3-bc32-94786d8fc91b \
        -DCMAKE_CXX_STDLIB_MODULES_JSON=$LLVM_PREFIX/lib/c++/libc++.modules.json ..
  ```

  It must be the first configure because the probed (broken) value is frozen
  into `CMakeFiles/<version>/CMakeCXXCompiler.cmake` and shadows a later `-D`;
  a build directory configured without it has to be wiped.

- With that, the whole package builds (106 targets; CMake builds `std.pcm`
  once per build tree and injects it into every scanning target), and all 12
  trackerless-network unit tests pass, including `DuplicateMessageDetectorTest`.
- The pre-existing `STREAMR_IMPORT_STD` scaffold in `StreamrModules.cmake`
  worked as designed; no build-system changes were needed for the host.

### Android (arm64-android, NDK r29): BUILDS, but only with four workarounds

The dependency chain was real: `vcpkg install --triplet=arm64-android` of the
root manifest (folly and all built fresh for Android), then the six monorepo
dependency packages configured/built for Android exactly as
`install.sh --android` does, then the package itself:

```
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DVCPKG_TARGET_TRIPLET=arm64-android \
      -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=24 \
      -DSTREAMR_IMPORT_STD=ON \
      -DCMAKE_EXPERIMENTAL_CXX_IMPORT_STD=451f2fe2-a8a2-47c3-bc32-94786d8fc91b \
      -DCMAKE_CXX_FLAGS="-D__BIONIC_CTYPE_INLINE=__inline -U_FORTIFY_SOURCE -pthread" ..
```

Four distinct obstacles, in the order they appeared:

1. **CMake cannot even detect the standard library for the NDK toolchain.**
   The plain configure fails at the generate step with: `The "CXX_MODULE_STD"
   property on target "streamr-trackerless-network" requires toolchain
   support ... Only 'libc++' and 'libstdc++' are supported`. Root cause,
   reproduced by hand: `CMakeDetermineCompilerId.cmake` preprocesses
   `Modules/CXX-DetectStdlib.h` with the bare compiler command **without the
   Android `--target`**; the NDK clang then targets the macOS host, cannot
   find `<version>`, the probe errors out and
   `CMAKE_CXX_STANDARD_LIBRARY` stays empty. The identical probe with
   `--target=aarch64-linux-android24` added prints
   `CMAKE-STDLIB-DETECT: libc++`. This is a CMake 4.3.4 limitation for
   cross-compilation and needs an upstream fix. Experiment-only bypass: after
   the failed configure, hand-edit the persisted
   `build/CMakeFiles/4.3.4/CMakeCXXCompiler.cmake` to
   `set(CMAKE_CXX_COMPILER_IMPORT_STD "23;26")`, clear the error message, and
   set `CMAKE_CXX_STDLIB_MODULES_JSON` to
   `$NDK/toolchains/llvm/prebuilt/darwin-x86_64/lib/libc++.modules.json`,
   then reconfigure. (Not committable — it edits generated files.)
2. **Bionic ctype linkage** — `-D__BIONIC_CTYPE_INLINE=__inline` as
   established in Stage 2, applied globally.
3. **Bionic fortify linkage** — the NDK toolchain file enables
   `-D_FORTIFY_SOURCE=2` by default, and the fortify wrappers
   (`bits/fortify/string.h` etc.) are *also* internal-linkage inlines that
   the std module cannot re-export (18 errors of the same shape as ctype:
   `using declaration referring to 'strrchr' with internal linkage cannot be
   exported`). The raw Stage-2 compiles missed this because they never
   defined `_FORTIFY_SOURCE`. Workaround `-U_FORTIFY_SOURCE` — which
   **disables a hardening feature**, making this the single worst item on
   the list for a production build.
4. **`-pthread` BMI mismatch on the std module** — the first build died with
   `POSIX thread support was disabled in AST file '...std.pcm' but is
   currently enabled` when `DuplicateMessageDetector.cppm` (which inherits
   `-pthread` from folly through the target's PUBLIC flags) tried to import
   the std BMI that CMake's synthesized `__CMAKE::CXX26` target had compiled
   without `-pthread`. This is the same Android-only BMI-configuration class
   of problem the repo already solved for its own modules (see the PUBLIC
   `-pthread` note in `StreamrModules.cmake`); for the experiment a global
   `-pthread` in `CMAKE_CXX_FLAGS` aligns the std BMI with its importers.

With all four in place the full package builds for Android: 106/106 targets,
`libstreamr-trackerless-network.a` plus the test executables as genuine
`ELF 64-bit LSB pie executable, ARM aarch64` binaries, with the converted
partition demonstrably importing the CMake-built `std.pcm` (31 MB, visible in
the target's module map). The binaries were not executed (no device attached);
Android tests are never executed in this repository's CI either.

## NDK r30 beta 1 (retested 2026-07-04)

The owner offered to switch the project to the NDK's latest pre-release if it
unblocked Android. Retested everything against **NDK r30 beta 1**
(`Pkg.Revision = 30.0.14904198-beta1`, clang 21.0.0 build r574158, from the
official `android-ndk-r30-beta1-darwin.dmg`). Verdict up front: **r30 beta 1
changes nothing that matters — both Bionic linkage blockers persist
unchanged, so switching NDKs does not enable `import std;` on Android.**
The retest did, however, produce one genuinely good piece of news about the
CMake wiring that applies to r29 as well (item 1 below).

Status of the four r29 workarounds on r30 beta 1:

1. **Hand-patching generated CMake files — NO LONGER NEEDED (on any NDK).**
   The CMake standard-library-detection defect is in CMake, unchanged — and a
   plain `-DCMAKE_CXX_STANDARD_LIBRARY=libc++` cache preset does *not* work,
   because `CMakeDetermineCompilerId.cmake` unconditionally overwrites the
   variable before probing. But the probe's command line includes the user's
   `CMAKE_CXX_FLAGS`, so adding the Android target there makes the probe
   itself succeed:

   ```
   -DCMAKE_CXX_FLAGS="--target=aarch64-none-linux-android24 ..."
   ```

   With that one flag, `CMAKE-STDLIB-DETECT` returns `libc++`, CMake resolves
   `libc++.modules.json` by itself through the NDK driver, and the whole
   import-std configure/build completes **with zero editing of generated
   files**. Verified end to end (std module built by CMake, `import std;`
   consumer, aarch64 ELF binary) on both r30 beta 1 and r29. The duplicated
   `--target` is harmless (identical value; the later occurrence wins). This
   is a clean, automatable, cache-variable-only workaround — the remaining
   CMake-side wish is only the upstream fix that would make even this flag
   unnecessary.
2. **`-D__BIONIC_CTYPE_INLINE=__inline` — STILL REQUIRED.** Bionic's
   `ctype.h` in r30 beta 1 carries the identical `static __inline` pattern.
   The out-of-the-box std module compile fails with the same 21 errors
   (`using declaration referring to 'isalnum' with internal linkage cannot
   be exported`, reported via `std/cctype.inc`). With the override, the std
   module compiles (31.3 MB BMI) and the folly/boost mixing test plus a
   linked aarch64 executable work exactly as on r29.
3. **`-U_FORTIFY_SOURCE` — STILL REQUIRED.** With the ctype override plus an
   explicit `-D_FORTIFY_SOURCE=2` (the NDK toolchain default), the compile
   still fails — 19 errors of the same internal-linkage shape, this time
   surfacing through the fortified stdio wrappers (`fgets`, `fread`,
   `fwrite` via `std/cstdio.inc`).
4. **Global `-pthread` — STILL REQUIRED.** Reproduced the identical
   `POSIX thread support was disabled in AST file '...std.pcm' but is
   currently enabled` error on r30 beta 1 with a minimal project whose
   consumer target adds `-pthread`. As expected: this is CMake failing to
   give its synthesized std-module target the importers' flags, independent
   of the NDK version.

Score: one of four workarounds eliminated — and that one is NDK-independent
(it was a deficiency of the first experiment's bypass, not of the NDK). The
two disqualifying Bionic overrides and the pthread flag remain. **There is
nothing to gain by switching the project to r30 beta 1.** If a future NDK
fixes Bionic's inline linkage, the minimal build-system changes for Android
`import std;` would be, concretely:

1. `STREAMR_IMPORT_STD=ON` plus the CMake-version UUID (already scaffolded in
   `StreamrModules.cmake`).
2. On Android, append `--target=aarch64-none-linux-android${ANDROID_PLATFORM}`
   to `CMAKE_CXX_FLAGS` at configure time (until CMake's stdlib probe learns
   to pass the toolchain target itself).
3. On Android, make `-pthread` global (or otherwise ensure the std BMI is
   compiled with the same thread mode as its importers) — the same class of
   fix as the existing PUBLIC `-pthread` note in `StreamrModules.cmake`.
4. On the macOS host, pass
   `-DCMAKE_CXX_STDLIB_MODULES_JSON=$LLVM_PREFIX/lib/c++/libc++.modules.json`
   on the first configure (until Homebrew LLVM's driver finds its own
   manifest).

## Recommendation

**Do not adopt `import std;` in the tree yet — not even host-only.**

- Android (the release platform) requires overriding a Bionic-internal macro
  globally *and* disabling `_FORTIFY_SOURCE`. Neither is a supported
  configuration; the first silently changes the linkage of libc functions for
  every TU, the second removes a hardening feature from shipped binaries.
  Shipping on top of that is not defensible today.
- CMake's standard-library detection probe runs without `--target`; the
  r30-beta-1 retest found a clean flags-only bypass (put the Android
  `--target` into `CMAKE_CXX_FLAGS`), so this is no longer a hand-edit
  blocker — but it is still an undocumented workaround for a CMake defect.
- A host-only adoption behind `STREAMR_IMPORT_STD` would fork the source
  style: the same partition cannot say both `import std;` and keep its
  includes, so every converted file would need `#ifdef`-free dual paths (not
  possible for module purviews) or the flag would stop being optional. Keep
  the flag as the experiment vehicle it already is.
- CMake still gates the feature behind a per-version experimental UUID, so
  every CMake upgrade (local and CI) needs a lock-step UUID bump.

What would change the recommendation — watch for these events:

1. An NDK release whose Bionic gives the ctype and fortify wrappers external
   linkage (or whose libc++ module sources are actually compile-tested
   against Bionic, fortify included); the r29 and r30 beta 1 changelogs are
   both silent about modules (and r30 beta 1 has been tested here — still
   broken), so test each new NDK with:
   `$NDK/bin/clang++ --target=aarch64-linux-android24 -std=c++26
   -D_FORTIFY_SOURCE=2 --precompile $NDK/share/libc++/v1/std.cppm -o
   /dev/null` (the `-D_FORTIFY_SOURCE=2` matters — without it the fortify
   failure stays hidden).
2. A CMake release whose standard-library detection passes the toolchain's
   `--target` (so `CMAKE_CXX_STANDARD_LIBRARY` is detected for Android
   cross-builds), and eventually promotion of `import std` out of
   experimental (no more UUID). Also Homebrew LLVM fixing
   `-print-library-module-manifest-path` / `-print-file-name=
   libc++.modules.json` so host discovery works without the manual
   `CMAKE_CXX_STDLIB_MODULES_JSON` path.
3. vcpkg gaining first-class import-std support so dependency and monorepo
   builds agree on flags.

If Android support matures, `StreamrModules.cmake` will additionally need to
propagate `-pthread` into the std-module BMI on Android (the same fix already
applied to the package's own module targets).

When those land, the conversion itself is mechanical and this branch is the
template: delete the std includes from a partition's global module fragment,
add `import std;` to the purview, and qualify any global-namespace C names
(`::int64_t` → `std::int64_t`, `::size_t` → `std::size_t`) that had been
leaking in textually.
