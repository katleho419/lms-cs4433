// Hands back the value a transaction body produced.
#ifndef LMS_SERVICE_TRANSACTIONRESULT_HPP
#define LMS_SERVICE_TRANSACTIONRESULT_HPP

#include <optional>
#include <stdexcept>
#include <utility>

namespace lms {

// A transaction body stores its result in `slot`. runInTransaction either runs the body to the end or
// throws, so the slot is always filled when it returns; the check turns a broken promise (a programming
// error) into a clear exception instead of undefined behaviour.
template <typename T>
T takeResult(std::optional<T>& slot) {
    if (!slot) {
        throw std::logic_error("The transaction finished without producing a result.");
    }
    return std::move(*slot);
}

}  // namespace lms

#endif
