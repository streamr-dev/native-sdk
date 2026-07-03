// Coarse façade partition over ALL public headers of streamr-utils.
// One partition (instead of one per header) keeps the number of
// module units — and thus repeated global-module-fragment parses
// of the expensive header stacks — minimal during the façade
// stage (measured at the Phase 2.4 bench checkpoint). Per-header
// granularity returns at consolidation if needed.
module;

#include "streamr-utils/AbortController.hpp"
#include "streamr-utils/AbortableTimers.hpp"
#include "streamr-utils/BinaryUtils.hpp"
#include "streamr-utils/Branded.hpp"
#include "streamr-utils/ENSName.hpp"
#include "streamr-utils/EnableSharedFromThis.hpp"
#include "streamr-utils/EthereumAddress.hpp"
#include "streamr-utils/Ipv4Helper.hpp"
#include "streamr-utils/ReplayEventEmitterWrapper.hpp"
#include "streamr-utils/RetryUtils.hpp"
#include "streamr-utils/SigningUtils.hpp"
#include "streamr-utils/StreamID.hpp"
#include "streamr-utils/StreamPartID.hpp"
#include "streamr-utils/Uuid.hpp"
#include "streamr-utils/collect.hpp"
#include "streamr-utils/partition.hpp"
#include "streamr-utils/runAndWaitForEvents.hpp"
#include "streamr-utils/toCoroTask.hpp"
#include "streamr-utils/toEthereumAddressOrENSName.hpp"
#include "streamr-utils/waitForCondition.hpp"
#include "streamr-utils/waitForEvent.hpp"

export module streamr.utils:all;

export namespace streamr::utils::abortsignalevents {

using streamr::utils::abortsignalevents::Aborted;

} // namespace streamr::utils::abortsignalevents

export namespace streamr::utils {

using streamr::utils::AbortController;
using streamr::utils::AbortSignal;
using streamr::utils::AbortSignalEvents;
using streamr::utils::abortsignalevents::Aborted;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::AbortableTimers;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::BinaryUtils;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::Branded;
using streamr::utils::BrandString;
using streamr::utils::IntegralType;
using streamr::utils::NonIntegralType;
using streamr::utils::operator""_brand;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::ENSName;
using streamr::utils::isENSNameFormatIgnoreCase;
using streamr::utils::toENSName;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::EnableSharedFromThis;
using streamr::utils::EnableSharedFromThisBase;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::EthereumAddress;
using streamr::utils::toEthereumAddress;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::Ipv4Helper;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::createReplayEventEmitterWrapper;
using streamr::utils::makeReplayEventEmitterWrapper;
using streamr::utils::ReplayEventEmitterWrapper;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::RetryUtils;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::SigningUtils;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::StreamID;
using streamr::utils::StreamIDUtils;
using streamr::utils::toStreamID;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::DELIMITER;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::toStreamPartID;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::Uuid;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::collect;
using streamr::utils::ResultType;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::ensureValidStreamPartitionCount;
using streamr::utils::ensureValidStreamPartitionIndex;
using streamr::utils::MAX_PARTITION_COUNT;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::runAndWaitForEvents;
using streamr::utils::runAndWaitForEventsDefaultTimeout;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::toCoroTask;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::toEthereumAddressOrENSName;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::ConditionMet;
using streamr::utils::defaultRetryInterval;
using streamr::utils::Poller;
using streamr::utils::PollerEvents;
using streamr::utils::waitForCondition;

} // namespace streamr::utils

export namespace streamr::utils {

using streamr::utils::defaultTimeout;
using streamr::utils::remove_pointer;
using streamr::utils::Waiter;
using streamr::utils::waitForEvent;

} // namespace streamr::utils
