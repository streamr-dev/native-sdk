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
