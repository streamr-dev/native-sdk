#ifndef STREAMR_UTILS_UUID_HPP
#define STREAMR_UTILS_UUID_HPP

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace streamr::utils {

class Uuid {
public:
    static std::string v4() {
        boost::uuids::uuid uuid;
        return boost::uuids::to_string(uuid);
    }
};

} // namespace streamr::utils

#endif // STREAMR_UTILS_UUID_HPP