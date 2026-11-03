// Unit tests: password hashing, password policy, users, roles and permissions.
#include <algorithm>
#include "TestSupport.hpp"
#include "domain/UserFactory.hpp"
#include "security/PasswordHasher.hpp"
#include "security/PasswordPolicy.hpp"

using namespace lms;

TEST(PasswordHasher, SaltIs16RandomBytesAsHex) {
    const std::string a = PasswordHasher::generateSalt();
    const std::string b = PasswordHasher::generateSalt();
    EXPECT_EQ(a.size(), 32u);
    EXPECT_NE(a, b);
    EXPECT_EQ(a.find_first_not_of("0123456789abcdef"), std::string::npos);
}

TEST(PasswordHasher, HashHasSelfDescribingFormatAndNeverContainsThePassword) {
    const std::string salt = PasswordHasher::generateSalt();
    const std::string hash = PasswordHasher::hash("Secret123", salt);
    EXPECT_EQ(hash.rfind("scrypt$32768$8$1$", 0), 0u);
    EXPECT_EQ(hash.find("Secret123"), std::string::npos);
    EXPECT_EQ(hash.size(), std::string("scrypt$32768$8$1$").size() + 64);
}

TEST(PasswordHasher, VerifyAcceptsRightAndRejectsWrongPassword) {
    const std::string salt = PasswordHasher::generateSalt();
    const std::string hash = PasswordHasher::hash("Secret123", salt);
    EXPECT_TRUE(PasswordHasher::verify("Secret123", salt, hash));
    EXPECT_FALSE(PasswordHasher::verify("secret123", salt, hash));
    EXPECT_FALSE(PasswordHasher::verify("", salt, hash));
}

TEST(PasswordHasher, SamePasswordWithDifferentSaltGivesDifferentHash) {
    const std::string h1 = PasswordHasher::hash("Secret123", PasswordHasher::generateSalt());
    const std::string h2 = PasswordHasher::hash("Secret123", PasswordHasher::generateSalt());
    EXPECT_NE(h1, h2);
}

TEST(PasswordHasher, BadSaltIsAnError) {
    EXPECT_THROW(PasswordHasher::hash("x", "zz"), LmsError);
    EXPECT_THROW(PasswordHasher::hash("x", ""), LmsError);
    EXPECT_THROW(PasswordHasher::hash("x", "abc"), LmsError);  // odd length
}

TEST(PasswordHasher, CorruptStoredValuesAreRejectedNotCrashed) {
    const std::string salt = PasswordHasher::generateSalt();
    const std::string key(64, 'a');
    const std::vector<std::string> bad = {
        "",
        "plaintext",
        "bcrypt$32768$8$1$" + key,
        "scrypt$32768$8$" + key,                 // too few parts
        "scrypt$32768$8$1$" + key + "$extra",    // too many parts
        "scrypt$abc$8$1$" + key,                 // non-numeric
        "scrypt$1000$8$1$" + key,                // N below minimum
        "scrypt$32769$8$1$" + key,               // N not a power of two
        "scrypt$262144$8$1$" + key,              // N above the safety limit
        "scrypt$32768$0$1$" + key,               // r = 0
        "scrypt$32768$17$1$" + key,              // r too big
        "scrypt$32768$8$0$" + key,               // p = 0
        "scrypt$32768$8$5$" + key,               // p too big
        "scrypt$32768$8$1$abcd",                 // key too short
        "scrypt$32768$8$1$" + std::string(64, 'z'),  // key not hex
        "scrypt$9999999999$8$1$" + key,          // number too long
    };
    for (const auto& s : bad) EXPECT_FALSE(PasswordHasher::verify("x", salt, s)) << s;
    EXPECT_FALSE(PasswordHasher::verify("x", "not-hex", "scrypt$32768$8$1$" + key));
}

TEST(PasswordPolicy, LengthBoundaries) {
    EXPECT_THROW_MSG(PasswordPolicy::validate("abcdef1", "u"), ValidationError, "at least 8");   // 7
    EXPECT_NO_THROW(PasswordPolicy::validate("abcdefg1", "u"));                                  // 8
    EXPECT_NO_THROW(PasswordPolicy::validate(std::string(127, 'a') + "1", "u"));                 // 128
    EXPECT_THROW_MSG(PasswordPolicy::validate(std::string(128, 'a') + "1", "u"), ValidationError, "at most 128");  // 129
}

TEST(PasswordPolicy, NeedsLetterAndDigit) {
    EXPECT_THROW_MSG(PasswordPolicy::validate("12345678", "u"), ValidationError, "letter and one digit");
    EXPECT_THROW_MSG(PasswordPolicy::validate("abcdefgh", "u"), ValidationError, "letter and one digit");
    EXPECT_NO_THROW(PasswordPolicy::validate("abcd1234", "u"));
}

TEST(PasswordPolicy, MayNotEqualUsernameIgnoringCase) {
    EXPECT_THROW_MSG(PasswordPolicy::validate("Student1", "student1"), ValidationError, "same as the username");
    EXPECT_NO_THROW(PasswordPolicy::validate("Student12", "student1"));
}

struct PermissionRow { Role role; std::vector<Permission> allowed; };

TEST(Permissions, EachRoleHoldsExactlyItsOwnPermissions) {
    const std::vector<Permission> all = {
        Permission::ManageAccounts, Permission::ManageDepartments, Permission::ManageCourses,
        Permission::AssignLecturers, Permission::EnrolStudents, Permission::CreateAssignment,
        Permission::GrantExtension, Permission::MarkSubmission, Permission::ModerateMarks,
        Permission::ReleaseMarks, Permission::ViewAuditHistory, Permission::SubmitWork, Permission::ViewOwnMarks};
    const std::vector<PermissionRow> table = {
        {Role::Admin, {Permission::ManageAccounts, Permission::ManageDepartments, Permission::ManageCourses,
                       Permission::AssignLecturers, Permission::EnrolStudents}},
        {Role::Lecturer, {Permission::CreateAssignment, Permission::GrantExtension, Permission::MarkSubmission}},
        {Role::Hod, {Permission::ModerateMarks, Permission::ReleaseMarks, Permission::ViewAuditHistory}},
        {Role::Student, {Permission::SubmitWork, Permission::ViewOwnMarks}}};
    test::World w;
    for (const auto& row : table) {
        UserPtr u = row.role == Role::Admin ? w.adminU : row.role == Role::Lecturer ? w.lec : row.role == Role::Hod ? w.hod : w.stu1;
        ASSERT_EQ(u->getRole(), row.role);
        for (Permission p : all) {
            const bool expected = std::find(row.allowed.begin(), row.allowed.end(), p) != row.allowed.end();
            EXPECT_EQ(u->can(p), expected) << toString(row.role) << " permission " << static_cast<int>(p);
        }
    }
}

TEST(Role, StringConversionsRoundTrip) {
    for (Role r : {Role::Admin, Role::Lecturer, Role::Hod, Role::Student}) {
        EXPECT_EQ(roleFromString(toString(r)), r);
        EXPECT_FALSE(displayName(r).empty());
    }
    EXPECT_EQ(displayName(Role::Hod), "Head of Department");
    EXPECT_THROW(roleFromString("PRINCIPAL"), ValidationError);
    EXPECT_THROW(roleFromString("admin"), ValidationError);  // exact match only
}

TEST(UserFactory, CreatesTheRightSubclassWithHashedPassword) {
    auto s = UserFactory::createUser(Role::Student, UserData{"thabo", "Thabo M", "Passw0rd1", "S100", 0});
    EXPECT_EQ(s->getRole(), Role::Student);
    EXPECT_NE(s->getPasswordHash(), "Passw0rd1");
    EXPECT_TRUE(s->verifyPassword("Passw0rd1"));
    EXPECT_FALSE(s->verifyPassword("Passw0rd2"));
    EXPECT_EQ(dynamic_cast<Student&>(*s).getStudentNumber(), "S100");

    auto l = UserFactory::createUser(Role::Lecturer, UserData{"lecturer", "Lee", "Passw0rd1", "", 3});
    EXPECT_EQ(l->getRole(), Role::Lecturer);
    EXPECT_EQ(l->getDepartmentId(), 3);
    auto h = UserFactory::createUser(Role::Hod, UserData{"hodhod", "Hod", "Passw0rd1", "", 4});
    EXPECT_EQ(h->getRole(), Role::Hod);
    auto a = UserFactory::createUser(Role::Admin, UserData{"root1", "Root", "Passw0rd1", "", 0});
    EXPECT_EQ(a->getRole(), Role::Admin);
    EXPECT_EQ(a->getDepartmentId(), 0);
}

TEST(UserFactory, TwoUsersWithTheSamePasswordGetDifferentSaltsAndHashes) {
    auto a = UserFactory::createUser(Role::Admin, UserData{"admin1", "A", "Passw0rd1", "", 0});
    auto b = UserFactory::createUser(Role::Admin, UserData{"admin2", "B", "Passw0rd1", "", 0});
    EXPECT_NE(a->getSalt(), b->getSalt());
    EXPECT_NE(a->getPasswordHash(), b->getPasswordHash());
}

TEST(UserFactory, ValidatesUsername) {
    auto make = [](const std::string& name) { return UserFactory::createUser(Role::Admin, UserData{name, "N", "Passw0rd1", "", 0}); };
    EXPECT_THROW_MSG(make("ab"), ValidationError, "3 to 32");                       // 2 chars
    EXPECT_NO_THROW(make("abc"));                                                   // 3 chars
    EXPECT_NO_THROW(make(std::string(32, 'a')));                                    // 32 chars
    EXPECT_THROW_MSG(make(std::string(33, 'a')), ValidationError, "3 to 32");       // 33 chars
    EXPECT_THROW_MSG(make("bad name"), ValidationError, "may only contain");
    EXPECT_THROW(make("a;b--"), ValidationError);
    EXPECT_THROW(make("x' OR '1'='1"), ValidationError);
    EXPECT_NO_THROW(make("a.b_c-d"));
    EXPECT_NO_THROW(make("  padded  "));  // trimmed
}

TEST(UserFactory, ValidatesOtherFields) {
    EXPECT_THROW(UserFactory::createUser(Role::Admin, UserData{"admin1", "   ", "Passw0rd1", "", 0}), ValidationError);
    EXPECT_THROW(UserFactory::createUser(Role::Admin, UserData{"admin1", "N", "short1", "", 0}), ValidationError);
    EXPECT_THROW_MSG(UserFactory::createUser(Role::Student, UserData{"stud1", "N", "Passw0rd1", "  ", 0}), ValidationError, "student number");
    EXPECT_THROW_MSG(UserFactory::createUser(Role::Lecturer, UserData{"lect1", "N", "Passw0rd1", "", 0}), ValidationError, "department");
    EXPECT_THROW(UserFactory::createUser(Role::Hod, UserData{"hod11", "N", "Passw0rd1", "", -1}), ValidationError);
}

TEST(UserFactory, RestoreDoesNotRehash) {
    UserRecord r;
    r.id = 7; r.username = "old"; r.passwordHash = "scrypt$32768$8$1$ab"; r.salt = "00"; r.fullName = "Old User";
    r.role = Role::Lecturer; r.departmentId = 2;
    auto u = UserFactory::restore(r);
    EXPECT_EQ(u->getId(), 7);
    EXPECT_EQ(u->getPasswordHash(), "scrypt$32768$8$1$ab");
    EXPECT_EQ(u->getDepartmentId(), 2);
    EXPECT_EQ(u->getRole(), Role::Lecturer);
    r.role = Role::Student; r.studentNumber = "S9";
    EXPECT_EQ(dynamic_cast<Student&>(*UserFactory::restore(r)).getStudentNumber(), "S9");
    r.role = Role::Hod;
    EXPECT_EQ(UserFactory::restore(r)->getRole(), Role::Hod);
    r.role = Role::Admin;
    EXPECT_EQ(UserFactory::restore(r)->getRole(), Role::Admin);
}

TEST(User, ChangePasswordChecksOldPasswordAndPolicyAndRotatesSalt) {
    auto u = UserFactory::createUser(Role::Admin, UserData{"admin1", "A", "Passw0rd1", "", 0});
    const std::string oldSalt = u->getSalt();
    const std::string oldHash = u->getPasswordHash();
    EXPECT_THROW(u->changePassword("wrong", "NewPassw0rd"), AuthenticationError);
    EXPECT_THROW(u->changePassword("Passw0rd1", "short1"), ValidationError);
    EXPECT_EQ(u->getPasswordHash(), oldHash);  // unchanged after failures
    u->changePassword("Passw0rd1", "NewPassw0rd");
    EXPECT_NE(u->getSalt(), oldSalt);
    EXPECT_TRUE(u->verifyPassword("NewPassw0rd"));
    EXPECT_FALSE(u->verifyPassword("Passw0rd1"));
}
