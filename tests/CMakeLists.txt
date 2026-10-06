add_executable(tests_unit
    unit/smoke_test.cpp
)

target_link_libraries(tests_unit
    PRIVATE
        nodepulse_lib
        GTest::gtest
        GTest::gtest_main
        pthread
)

target_compile_features(tests_unit PRIVATE cxx_std_20)

if(WARNINGS_AS_ERRORS)
    target_compile_options(tests_unit PRIVATE -Wall -Wextra -Wpedantic -Werror)
else()
    target_compile_options(tests_unit PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GoogleTest)
gtest_discover_tests(tests_unit)

