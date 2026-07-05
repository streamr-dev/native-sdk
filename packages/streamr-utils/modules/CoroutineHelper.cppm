// Module streamr.utils.CoroutineHelper
//
// THE single owner of folly's coroutine header stack (MODERNIZATION.md,
// third-party wrapper modules). Before this module existed, ~28 module
// units and ~21 test files each re-parsed these headers textually in
// their global module fragments — the dominant clean-build cost measured
// at the end of the Phase 2.6 consolidation. Importers now load one
// prebuilt module interface instead.
//
// The names are re-exported in their ORIGINAL namespaces
// (folly::coro::Task stays folly::coro::Task), so use sites are
// unchanged; only the include is replaced by an import.
//
// RULE (C-5 miscompile, MODERNIZATION.md): a translation unit must not
// BOTH include folly coroutine headers textually AND import a module
// whose global module fragment includes them — overlapping declaration
// merging miscompiled coroutine resumption. This module exists so that
// no other unit needs the textual includes; do not reintroduce them.
module;

#include <utility>

#include <folly/CancellationToken.h>
#include <folly/Unit.h>
#include <folly/experimental/coro/BlockingWait.h>
#include <folly/experimental/coro/Collect.h>
#include <folly/experimental/coro/DetachOnCancel.h>
#include <folly/experimental/coro/Invoke.h>
#include <folly/experimental/coro/Promise.h>
#include <folly/experimental/coro/Sleep.h>
#include <folly/experimental/coro/Task.h>
#include <folly/experimental/coro/Timeout.h>
#include <folly/experimental/coro/ViaIfAsync.h>
#include <folly/futures/Future.h>

export module streamr.utils.CoroutineHelper;

export namespace folly::coro {

using folly::coro::co_invoke;
using folly::coro::collectAll;
using folly::coro::collectAllRange;
using folly::coro::detachOnCancel;
using folly::coro::Future;
using folly::coro::makePromiseContract;
using folly::coro::Promise;
using folly::coro::semi_await_result_t;
using folly::coro::sleep;
using folly::coro::Task;
using folly::coro::timeout;

} // namespace folly::coro

export namespace folly {

// Companions that use sites spell and that used to arrive transitively
// through the coroutine headers.
using folly::CancellationSource;
using folly::CancellationToken;
using folly::FutureCancellation;
using folly::FutureTimeout;
using folly::OperationCancelled;
using folly::Unit;

} // namespace folly

export namespace streamr::utils {

// folly's blockingWait / co_withExecutor / co_withCancellation are
// customization-point objects that cannot be re-exported by
// using-declaration (internal linkage or conflicting redeclarations
// across folly headers), so they are exported as perfect-forwarding
// shims here; use sites spell streamr::utils::X instead of
// folly::coro::X.
template <typename... Args>
inline auto blockingWait(Args&&... args)
    -> decltype(folly::coro::blockingWait(std::forward<Args>(args)...)) {
    return folly::coro::blockingWait(std::forward<Args>(args)...);
}

template <typename... Args>
inline auto co_withExecutor(
    Args&&... args) // NOLINT(readability-identifier-naming)
    -> decltype(folly::coro::co_withExecutor(std::forward<Args>(args)...)) {
    return folly::coro::co_withExecutor(std::forward<Args>(args)...);
}

template <typename... Args>
inline auto co_withCancellation(
    Args&&... args) // NOLINT(readability-identifier-naming)
    -> decltype(folly::coro::co_withCancellation(std::forward<Args>(args)...)) {
    return folly::coro::co_withCancellation(std::forward<Args>(args)...);
}

} // namespace streamr::utils
