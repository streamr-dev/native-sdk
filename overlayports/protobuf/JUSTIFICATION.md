# Why this overlay exists

Baseline: vcpkg 2026.06.24 port of protobuf v33.4, plus ONE local patch.

## fix-static-varint-parse-for-cxx-modules.patch

`src/google/protobuf/parse_context.h` declares the two
`VarintParseSlowArm(...)` overloads as `static` (internal linkage). When a
protobuf header is included in the *global module fragment* of a C++
module unit (the monorepo's `:protos` façade partitions —
MODERNIZATION.md Part 2), Clang 22 performs the implicit instantiation of
`VarintParse<...>` in module-purview context, where TU-local (internal
linkage) declarations from the GMF are not usable — the build fails with
"no matching function for call to 'VarintParseSlowArm'" on every arm64
platform (macOS, iOS, Android, arm64-linux).

Minimal repro (fails without the patch, compiles with it):

```cpp
module;
#include <google/protobuf/any.pb.h>
export module repro;
```

The patch changes `static` → `inline` on the two overloads: external
linkage (usable from module purview), ODR-safe because the definitions
are identical in every TU. No behavior change.

Delete this overlay when upstream protobuf makes its headers module-GMF
clean (or switches these helpers to `inline`).
