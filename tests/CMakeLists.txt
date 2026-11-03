# GoogleTest: use the system copy if installed (sudo apt install libgtest-dev),
# otherwise download it.
find_package(GTest QUIET)
if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

add_executable(lms_tests
  test_util.cpp
  test_security.cpp
  test_domain.cpp
  test_data.cpp
  test_services.cpp
  test_system.cpp)
target_include_directories(lms_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lms_tests PRIVATE lms_app lms_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(lms_tests DISCOVERY_TIMEOUT 60)
