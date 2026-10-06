add_library(nodepulse_lib STATIC
    config/config.cpp
    collectors/cpu_collector.cpp
    collectors/disk_collector.cpp
    collectors/memory_collector.cpp
    collectors/system_collector.cpp
    controllers/cpu_controller.cpp
    controllers/disk_controller.cpp
    controllers/health_controller.cpp
    controllers/memory_controller.cpp
    controllers/system_controller.cpp
    services/cpu_service.cpp
    services/disk_service.cpp
    services/memory_service.cpp
    services/system_service.cpp
    server/server.cpp
    utils/error_response.cpp
    utils/logger.cpp
)

target_include_directories(nodepulse_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(nodepulse_lib
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        Drogon::Drogon
)

target_compile_features(nodepulse_lib PUBLIC cxx_std_20)

if(WARNINGS_AS_ERRORS)
    target_compile_options(nodepulse_lib PRIVATE -Wall -Wextra -Wpedantic -Werror)
else()
    target_compile_options(nodepulse_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

