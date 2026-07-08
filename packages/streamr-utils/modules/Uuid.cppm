// Module streamr.utils.Uuid
// CONSOLIDATED from the former header
// streamr-utils/Uuid.hpp (MODERNIZATION.md Phase 2.6):
// this file is now the source of truth.
module;

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

export module streamr.utils.Uuid;

import std;

export namespace streamr::utils {

class Uuid {
public:
    static std::string v4() {
        boost::uuids::uuid uuid = boost::uuids::random_generator()();
        return boost::uuids::to_string(uuid);
    }
};

} // namespace streamr::utils