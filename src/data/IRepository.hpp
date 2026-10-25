// Generic repository contract (design: Figure 1d).
#ifndef LMS_DATA_IREPOSITORY_HPP
#define LMS_DATA_IREPOSITORY_HPP

#include <optional>

namespace lms {

template <typename T>
class IRepository {
public:
    virtual ~IRepository() = default;
    // Returns the entity, or an empty optional when no row has that id.
    virtual std::optional<T> findById(int id) const = 0;
    // Inserts when entity.getId() == 0 (and then sets the new id), otherwise updates.
    virtual void save(T& entity) = 0;
};

}  // namespace lms

#endif
