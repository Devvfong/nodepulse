add_library(nodepulse_lib STATIC
    config/config.cpp
    collectors/cpu_collector.cpp
    collectors/disk_collector.cpp
    collectors/docker_collector.cpp
    collectors/memory_collector.cpp
    collectors/network_collector.cpp
    collectors/process_collector.cpp
    collectors/service_collector.cpp
    collectors/system_collector.cpp
    controllers/cpu_controller.cpp
    controllers/disk_controller.cpp
    controllers/docker_controller.cpp
    controllers/events_controller.cpp
    controllers/health_controller.cpp
    controllers/memory_controller.cpp
    controllers/metrics_controller.cpp
    controllers/network_controller.cpp
    controllers/process_controller.cpp
    controllers/service_controller.cpp
    controllers/system_controller.cpp
    repositories/postgres_repository.cpp
    services/cpu_service.cpp
    services/disk_service.cpp
    services/docker_service.cpp
    services/history_service.cpp
    services/memory_service.cpp
    services/metrics_exporter.cpp
    services/network_service.cpp
    services/process_service.cpp
    services/service_manager_service.cpp
    services/stream_service.cpp
    services/system_service.cpp
    server/server.cpp
    middleware/auth_filter.cpp
    middleware/rate_limit_filter.cpp
    utils/error_response.cpp
    utils/logger.cpp
    utils/security.cpp
)

target_include_directories(nodepulse_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        /home/devqii/.local/include
        /home/devqii/.local/include/postgresql
)

target_link_libraries(nodepulse_lib
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        Drogon::Drogon
        /home/devqii/.local/lib/x86_64-linux-gnu/libpqxx.a
        /home/devqii/.local/lib/x86_64-linux-gnu/libpq.so
        ${CMAKE_DL_LIBS}
)

target_compile_features(nodepulse_lib PUBLIC cxx_std_20)

if(WARNINGS_AS_ERRORS)
    target_compile_options(nodepulse_lib PRIVATE -Wall -Wextra -Wpedantic -Werror)
else()
    target_compile_options(nodepulse_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

