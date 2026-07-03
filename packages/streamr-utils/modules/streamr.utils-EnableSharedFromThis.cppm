// Façade partition over streamr-utils/EnableSharedFromThis.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-utils/EnableSharedFromThis.hpp"

export module streamr.utils:EnableSharedFromThis;

export namespace streamr::utils {

using streamr::utils::EnableSharedFromThis;
using streamr::utils::EnableSharedFromThisBase;

} // namespace streamr::utils
