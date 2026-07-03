// Façade partition over streamr-utils/ENSName.hpp (see the streamr.eventemitter
// partitions for the pattern rationale).
module;

#include "streamr-utils/ENSName.hpp"

export module streamr.utils:ENSName;

export namespace streamr::utils {

using streamr::utils::ENSName;
using streamr::utils::isENSNameFormatIgnoreCase;
using streamr::utils::toENSName;

} // namespace streamr::utils
