// Module streamr.utils.ExecutorHelper
//
// Single owner of folly's executor/scheduler headers (see
// CoroutineHelper.cppm for the rationale and the C-5 rule). Names are
// re-exported in their original namespace.
module;

#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/FunctionScheduler.h>

export module streamr.utils.ExecutorHelper;

export namespace folly {

using folly::CPUThreadPoolExecutor;
using folly::FunctionScheduler;

} // namespace folly
