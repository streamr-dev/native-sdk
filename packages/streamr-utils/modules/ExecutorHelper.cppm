// Module streamr.utils.ExecutorHelper
//
// Single owner of folly's executor/scheduler headers (see
// CoroutineHelper.cppm for the rationale and the C-5 rule). Names are
// re-exported in their original namespace.
module;

#include <folly/executors/CPUThreadPoolExecutor.h>
#include <folly/executors/FunctionScheduler.h>
#include <folly/executors/thread_factory/NamedThreadFactory.h>
#include <folly/executors/thread_factory/PriorityThreadFactory.h>

export module streamr.utils.ExecutorHelper;

export namespace folly {

using folly::CPUThreadPoolExecutor;
using folly::FunctionScheduler;
using folly::NamedThreadFactory;
using folly::PriorityThreadFactory;
using folly::ThreadFactory;

} // namespace folly
