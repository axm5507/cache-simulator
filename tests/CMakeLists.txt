include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
# Prevent Google Test from overriding our compiler/linker settings on Windows.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(cache_sim_tests
    test_cacheline.cpp
    test_cacheset.cpp
    test_cache_address.cpp
    test_cache_sim.cpp
)

target_link_libraries(cache_sim_tests
    PRIVATE
    cache_sim_lib
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(cache_sim_tests)
