cmake_minimum_required(VERSION 3.16)
project(LMS VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(LMS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(LMS_ENABLE_TEST_CLOCK
       "Allow the LMS_FAKE_NOW environment variable to set the clock. FOR TESTING ONLY: never enable in a release build."
       OFF)
option(LMS_BUILD_TESTS "Build the smoke tests" ON)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL 3.0 REQUIRED)

# Embed db/lms_schema.sql in the program so the database can create itself.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/db/lms_schema.sql" LMS_SCHEMA_SQL)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SchemaSql.hpp.in"
               "${CMAKE_CURRENT_BINARY_DIR}/generated/SchemaSql.hpp" @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/db/lms_schema.sql")

add_library(lms_core STATIC
  src/util/DateTime.cpp
  src/util/Validation.cpp
  src/security/PasswordHasher.cpp
  src/domain/Role.cpp
  src/domain/User.cpp
  src/domain/UserFactory.cpp
  src/domain/Submission.cpp
  src/domain/PenaltyPolicies.cpp
  src/data/Database.cpp
  src/data/SqliteUserRepository.cpp
  src/data/SqliteOrgRepository.cpp
  src/data/SqliteAssignmentRepository.cpp
  src/data/SqliteSubmissionRepository.cpp
  src/data/SqliteAuditRepository.cpp
  src/service/AdminService.cpp
  src/service/AssignmentService.cpp
  src/service/AuditService.cpp
  src/service/AuthService.cpp
  src/service/CourseService.cpp
  src/service/MarkingService.cpp
  src/service/ModerationService.cpp
  src/service/SubmissionService.cpp
)
target_include_directories(lms_core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(lms_core PUBLIC SQLite::SQLite3 OpenSSL::Crypto)

if(MSVC)
  set(LMS_WARNING_FLAGS /W4)
  if(LMS_WARNINGS_AS_ERRORS)
    list(APPEND LMS_WARNING_FLAGS /WX)
  endif()
else()
  set(LMS_WARNING_FLAGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
  if(LMS_WARNINGS_AS_ERRORS)
    list(APPEND LMS_WARNING_FLAGS -Werror)
  endif()
endif()
target_compile_options(lms_core PRIVATE ${LMS_WARNING_FLAGS})

add_library(lms_app STATIC src/ui/ConsoleApp.cpp)
target_link_libraries(lms_app PUBLIC lms_core)
target_compile_options(lms_app PRIVATE ${LMS_WARNING_FLAGS})

add_executable(lms src/app/main.cpp)
target_link_libraries(lms PRIVATE lms_app)
target_compile_options(lms PRIVATE ${LMS_WARNING_FLAGS})
if(LMS_ENABLE_TEST_CLOCK)
  target_compile_definitions(lms PRIVATE LMS_ENABLE_TEST_CLOCK)
endif()
install(TARGETS lms RUNTIME DESTINATION bin)
