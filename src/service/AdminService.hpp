// Administrator use cases: accounts, departments, courses, staffing and enrolment.
// Added during implementation; the class diagram (Figure 1c) shows the other six services.
#ifndef LMS_SERVICE_ADMINSERVICE_HPP
#define LMS_SERVICE_ADMINSERVICE_HPP

#include <string>
#include <vector>

#include "data/RepositoryInterfaces.hpp"
#include "domain/UserFactory.hpp"

namespace lms {

class AdminService {
public:
    AdminService(IUserRepository& users, IDepartmentRepository& departments, ICourseRepository& courses)
        : users_(users), departments_(departments), courses_(courses) {}

    UserPtr createUser(const User& actor, Role role, const UserData& data);
    std::vector<UserPtr> listUsers(const User& actor) const;

    Department createDepartment(const User& actor, const std::string& name);
    Course createCourse(const User& actor, const std::string& code, const std::string& title, int departmentId);

    // The lecturer must work in the course's department.
    void assignLecturer(const User& actor, int courseId, int lecturerId);
    void enrolStudent(const User& actor, int courseId, int studentId);

private:
    UserPtr requireUserWithRole(int userId, Role role) const;

    IUserRepository& users_;
    IDepartmentRepository& departments_;
    ICourseRepository& courses_;
};

}  // namespace lms

#endif
