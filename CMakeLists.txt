cmake_minimum_required(VERSION 3.22)

project(nodepulse
    VERSION 0.1.0
    DESCRIPTION "Lightweight Linux server monitoring and management agent written in C++20 using Drogon"
    LANGUAGES C CXX
)

# Standard C++20 Configuration
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build Options
option(BUILD_TESTING "Build test suites" ON)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis during compilation" OFF)

# Global Compiler Flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(WARNINGS_AS_ERRORS)
        add_compile_options(-Werror)
    endif()
    if(DEFINED ENV{HOME})
        add_compile_options(-ffile-prefix-map=$ENV{HOME}=/usr -ffile-prefix-map=$ENV{HOME}/.local=/usr)
    endif()
endif()

# Sanitizer Configuration
if(ENABLE_SANITIZERS)
    if(ENABLE_TSAN)
        message(FATAL_ERROR "ENABLE_SANITIZERS and ENABLE_TSAN are mutually exclusive.")
    endif()
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
elseif(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
    add_link_options(-fsanitize=thread)
endif()

# Find Dependencies
find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Drogon REQUIRED)
find_package(PostgreSQL REQUIRED)

# Portable pqxx discovery
find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PQXX QUIET libpqxx)
endif()
if(NOT PQXX_FOUND)
    find_path(PQXX_INCLUDE_DIRS NAMES pqxx/pqxx)
    find_library(PQXX_LIBRARIES NAMES pqxx libpqxx)
endif()

# Clang-Tidy Integration during compilation
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy)
    if(CLANG_TIDY_EXE)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    else()
        message(WARNING "ENABLE_CLANG_TIDY is ON but clang-tidy executable was not found.")
    endif()
endif()

# Quality Tooling Custom Targets
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    file(GLOB_RECURSE ALL_CXX_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/apps/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp
    )
    add_custom_target(format-check
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror ${ALL_CXX_SOURCE_FILES}
        COMMENT "Checking code formatting with clang-format"
    )
    add_custom_target(format-fix
        COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_CXX_SOURCE_FILES}
        COMMENT "Reformatting code with clang-format"
    )
endif()

find_program(CLANG_TIDY_RUNNER NAMES clang-tidy)
if(CLANG_TIDY_RUNNER)
    file(GLOB_RECURSE SRC_FILES_FOR_TIDY
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/apps/*.cpp
    )
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_RUNNER} -p ${CMAKE_BINARY_DIR} ${SRC_FILES_FOR_TIDY}
        COMMENT "Running static analysis with clang-tidy"
    )
endif()

# Subdirectories
add_subdirectory(src)
add_subdirectory(apps)

if(BUILD_TESTING)
    enable_testing()
    find_package(GTest REQUIRED)
    add_subdirectory(tests)
endif()

# Installation Rules (Phase 15)
include(GNUInstallDirs)

set(NODEPULSE_SYSCONFDIR "/etc" CACHE PATH "System configuration directory (default: /etc)")
set(NODEPULSE_CONFIG_DIR "${NODEPULSE_SYSCONFDIR}/nodepulse")
set(NODEPULSE_CONFIG_FILE "${NODEPULSE_CONFIG_DIR}/config.json")
set(NODEPULSE_INSTALL_FULL_BINDIR "${CMAKE_INSTALL_FULL_BINDIR}")

if(NOT DEFINED SYSTEMD_UNIT_DIR)
    set(SYSTEMD_UNIT_DIR "lib/systemd/system" CACHE PATH "Directory for systemd service units")
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/systemd/nodepulse.service.in
    ${CMAKE_CURRENT_BINARY_DIR}/infrastructure/systemd/nodepulse.service
    @ONLY
)

install(FILES config/config.example.json
    DESTINATION ${NODEPULSE_SYSCONFDIR}/nodepulse
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/infrastructure/systemd/nodepulse.service
    DESTINATION ${SYSTEMD_UNIT_DIR}
)

install(CODE "
    set(_svc_path \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${SYSTEMD_UNIT_DIR}/nodepulse.service\")
    if(EXISTS \"\${_svc_path}\")
        file(READ \"\${_svc_path}\" _svc_content)
        string(REGEX REPLACE \"ExecStart=[^ \t\r\n]+/nodepulse_server\" \"ExecStart=\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/nodepulse_server\" _svc_content \"\${_svc_content}\")
        file(WRITE \"\${_svc_path}\" \"\${_svc_content}\")
    endif()
")
