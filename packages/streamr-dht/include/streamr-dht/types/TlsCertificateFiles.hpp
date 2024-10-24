#ifndef STREAMR_DHT_TYPES_TLSCERTIFICATEFILES_HPP
#define STREAMR_DHT_TYPES_TLSCERTIFICATEFILES_HPP

#include <string>

namespace streamr::dht::types {

struct TlsCertificateFiles {
    std::string privateKeyFileName;
    std::string certFileName;
};

} // namespace streamr::dht::types

#endif // STREAMR_DHT_TYPES_TLSCERTIFICATEFILES_HPP