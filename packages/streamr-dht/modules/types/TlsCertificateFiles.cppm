// Module streamr.dht.TlsCertificateFiles
// CONSOLIDATED from the former header streamr-dht/types/TlsCertificateFiles.hpp
// (MODERNIZATION.md Phase 2.6): this file is now the source of truth.
module;

#include <string>

export module streamr.dht.TlsCertificateFiles;
export namespace streamr::dht::types {

struct TlsCertificateFiles {
    std::string privateKeyFileName;
    std::string certFileName;
};

} // namespace streamr::dht::types
