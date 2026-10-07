
# Unit Tests Target
add_executable(tests_unit
    unit/smoke_test.cpp
    unit/config_test.cpp
    unit/error_response_test.cpp
    unit/system_collector_test.cpp
    unit/cpu_collector_test.cpp
    unit/memory_collector_test.cpp
    unit/disk_collector_test.cpp
    unit/network_collector_test.cpp
    unit/process_collector_test.cpp
    unit/service_collector_test.cpp
    unit/security_test.cpp
    unit/auth_filter_test.cpp
    unit/rate_limit_filter_test.cpp
    unit/docker_collector_test.cpp
    unit/stream_service_test.cpp
    unit/metrics_exporter_test.cpp
    unit/postgres_repository_test.cpp
    unit/history_service_test.cpp
)

target_link_libraries(tests_unit
    PRIVATE
        nodepulse_lib
        Drogon::Drogon
        GTest::gtest
        GTest::gtest_main
        pthread
)

target_compile_features(tests_unit PRIVATE cxx_std_20)
target_compile_definitions(tests_unit PRIVATE NODEPULSE_TEST_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

if(WARNINGS_AS_ERRORS)
    target_compile_options(tests_unit PRIVATE -Wall -Wextra -Wpedantic -Werror)
else()
    target_compile_options(tests_unit PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Integration Tests Target
add_executable(tests_integration
    integration/http_integration_test.cpp
)

target_link_libraries(tests_integration
    PRIVATE
        nodepulse_lib
        Drogon::Drogon
        GTest::gtest
        GTest::gtest_main
        pthread
)

target_compile_features(tests_integration PRIVATE cxx_std_20)

if(WARNINGS_AS_ERRORS)
    target_compile_options(tests_integration PRIVATE -Wall -Wextra -Wpedantic -Werror)
else()
    target_compile_options(tests_integration PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GoogleTest)
gtest_discover_tests(tests_unit
    DISCOVERY_MODE PRE_TEST
)
gtest_discover_tests(tests_integration
    DISCOVERY_MODE PRE_TEST
)
