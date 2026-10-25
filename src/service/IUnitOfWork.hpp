// Lets services group several repository calls into one all-or-nothing
// transaction without knowing anything about SQLite (dependency inversion).
#ifndef LMS_SERVICE_IUNITOFWORK_HPP
#define LMS_SERVICE_IUNITOFWORK_HPP

#include <functional>

namespace lms {

class IUnitOfWork {
public:
    virtual ~IUnitOfWork() = default;
    // Runs `work`; if it throws, every change it made is undone and the
    // exception is rethrown.
    virtual void runInTransaction(const std::function<void()>& work) = 0;
};

}  // namespace lms

#endif
