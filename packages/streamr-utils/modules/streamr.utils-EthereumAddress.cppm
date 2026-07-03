// Façade partition over streamr-utils/EthereumAddress.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-utils/EthereumAddress.hpp"

export module streamr.utils:EthereumAddress;

export namespace streamr::utils {

using streamr::utils::EthereumAddress;
using streamr::utils::toEthereumAddress;

} // namespace streamr::utils
