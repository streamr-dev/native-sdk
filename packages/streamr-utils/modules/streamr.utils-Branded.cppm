// Façade partition over streamr-utils/Branded.hpp (see the
// streamr.eventemitter partitions for the pattern rationale).
module;

#include "streamr-utils/Branded.hpp"

export module streamr.utils:Branded;

export namespace streamr::utils {

using streamr::utils::Branded;
using streamr::utils::BrandString;
using streamr::utils::IntegralType;
using streamr::utils::NonIntegralType;
using streamr::utils::operator""_brand;

} // namespace streamr::utils
