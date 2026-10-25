// Abstract User and its four concrete roles (design: Figure 1a).
// Polymorphism is used for role identity and for permissions: every subclass
// answers can(permission) for itself, so services never test roles with
// if/else chains.
#ifndef LMS_DOMAIN_USER_HPP
#define LMS_DOMAIN_USER_HPP

#include <string>

#include "domain/Role.hpp"

namespace lms {

class User {
public:
    User(int id, std::string username, std::string passwordHash, std::string salt, std::string fullName);
    virtual ~User() = default;
    User(const User&) = delete;
    User& operator=(const User&) = delete;

    virtual Role getRole() const = 0;
    virtual bool can(Permission permission) const = 0;
    // Department of a lecturer or head of department; 0 for other roles.
    virtual int getDepartmentId() const { return 0; }

    int getId() const { return id_; }
    const std::string& getUsername() const { return username_; }
    const std::string& getFullName() const { return fullName_; }
    const std::string& getPasswordHash() const { return passwordHash_; }
    const std::string& getSalt() const { return salt_; }

    // Called by the repository after INSERT.
    void assignId(int id) { id_ = id; }

    bool verifyPassword(const std::string& plain) const;
    // Checks the old password and the password policy, then stores a new salt
    // and hash. The caller is responsible for saving the user.
    void changePassword(const std::string& oldPassword, const std::string& newPassword);

private:
    int id_;
    std::string username_;
    std::string passwordHash_;
    std::string salt_;
    std::string fullName_;
};

class Administrator : public User {
public:
    using User::User;
    Role getRole() const override { return Role::Admin; }
    bool can(Permission permission) const override;
};

class Lecturer : public User {
public:
    Lecturer(int id, std::string username, std::string passwordHash, std::string salt,
             std::string fullName, int departmentId);
    Role getRole() const override { return Role::Lecturer; }
    bool can(Permission permission) const override;
    int getDepartmentId() const override { return departmentId_; }

private:
    int departmentId_;
};

class HeadOfDepartment : public User {
public:
    HeadOfDepartment(int id, std::string username, std::string passwordHash, std::string salt,
                     std::string fullName, int departmentId);
    Role getRole() const override { return Role::Hod; }
    bool can(Permission permission) const override;
    int getDepartmentId() const override { return departmentId_; }

private:
    int departmentId_;
};

class Student : public User {
public:
    Student(int id, std::string username, std::string passwordHash, std::string salt,
            std::string fullName, std::string studentNumber);
    Role getRole() const override { return Role::Student; }
    bool can(Permission permission) const override;
    const std::string& getStudentNumber() const { return studentNumber_; }

private:
    std::string studentNumber_;
};

}  // namespace lms

#endif
