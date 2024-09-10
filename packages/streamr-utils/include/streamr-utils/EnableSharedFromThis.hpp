#ifndef ENABLE_SHARED_FROM_THIS_HPP
#define ENABLE_SHARED_FROM_THIS_HPP

#include <memory>

namespace streamr::utils {

// The base class is needed to allow multiple inheritance from
// EnableSharedFromThis.
struct EnableSharedFromThisBase
    : public std::enable_shared_from_this<EnableSharedFromThisBase> {
    virtual ~EnableSharedFromThisBase() = default;
};

// EnableSharedFromThis class should always be used instead of
// std::enable_shared_from_this.
// std::enable_shared_from_this is not compatible with inheritance
// and this class fixes the problem.
// **Important**: The classes that inherit from EnableSharedFromThis MUST
// always be used through a shared_ptr, and thus they MUST NOT have public
// constructors.

struct EnableSharedFromThis : virtual public EnableSharedFromThisBase {
    template <typename Self>
    auto sharedFromThis() {
        return std::dynamic_pointer_cast<Self>(
            EnableSharedFromThisBase::shared_from_this());
    }
    std::shared_ptr<EnableSharedFromThisBase> shared_from_this() = delete;
};

} // namespace streamr::utils

#endif
