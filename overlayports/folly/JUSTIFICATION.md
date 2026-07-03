# Why this overlay exists

Baseline: vcpkg 2026.06.24 (folly 2026.02.23.00). Identical to the upstream
port except one added patch:

- `fix-missing-exception-include.patch`: folly/result/rich_error_code.h uses
  `std::exception_ptr` without including `<exception>`; libc++ 22's strict
  transitive includes make this a hard compile error. Remove when a folly
  version with the fix reaches the pinned baseline.

- `-DFOLLY_NO_EXCEPTION_TRACER=ON` (configure option): folly 2026.02's
  exception tracer interposes `__cxa_throw` and, unlike in 2024-era folly
  (whose tracer was glibc-only and compiled to empty objects on libc++),
  activates on macOS — breaking `std::current_exception()` process-wide
  (returns null inside catch blocks), which aborts every folly::coro error
  path ("Cannot use throw_exception with an empty folly::exception_wrapper").
  Verified with a minimal repro. The SDK does not use the tracer.

## fix-anon-namespace-in-header-for-cxx-modules.patch (added Phase 2.4)

`folly/fibers/FiberManagerInternal-inl.h` defines helper function templates
(`runNoInline`, `tryEmplaceWithNoInline`, `preprocessOptions`) in an
ANONYMOUS namespace inside a header. When the header is reached both
through a C++ module's global module fragment (the monorepo's façade
partitions include folly coro headers) and textually in an importing
translation unit, Clang 22 emits both internal-linkage instantiations with
the SAME mangled name and fails in Release builds:

    error: definition with same mangled name
    '_ZN5folly6fibers12_GLOBAL__N_111runNoInline...' as another definition

The patch names the namespace (`fiber_manager_internal_detail`) and adds a
`using namespace` so call sites are unchanged — external linkage makes the
definitions ODR-mergeable (identical in every TU). Same disease class as
the protobuf `VarintParseSlowArm` overlay patch (anonymous-namespace /
static definitions in headers are incompatible with GMF+textual mixing).

Delete when upstream folly stops using anonymous namespaces in headers.
