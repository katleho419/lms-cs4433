#include "domain/Role.hpp"

#include "util/Errors.hpp"

namespace lms {

std::string toString(Role role) {
    switch (role) {
        case Role::Admin: return "ADMIN";
        case Role::Lecturer: return "LECTURER";
        case Role::Hod: return "HOD";
        case Role::Student: return "STUDENT";
    }
    throw ValidationError("Unknown role.");
}

Role roleFromString(const std::string& text) {
    if (text == "ADMIN") return Role::Admin;
    if (text == "LECTURER") return Role::Lecturer;
    if (text == "HOD") return Role::Hod;
    if (text == "STUDENT") return Role::Student;
    throw ValidationError("Unknown role: " + text);
}

std::string displayName(Role role) {
    switch (role) {
        case Role::Admin: return "Administrator";
        case Role::Lecturer: return "Lecturer";
        case Role::Hod: return "Head of Department";
        case Role::Student: return "Student";
    }
    throw ValidationError("Unknown role.");
}

}  // namespace lms
