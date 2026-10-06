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
set(CMAKE_BUILD_RPATH
    /home/devqii/.local/lib/x86_64-linux-gnu
    /home/devqii/.local/lib
)
set(CMAKE_INSTALL_RPATH
    /home/devqii/.local/lib/x86_64-linux-gnu
    /home/devqii/.local/lib
)
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

link_directories(
    $ENV{HOME}/.local/lib/x86_64-linux-gnu
    $ENV{HOME}/.local/lib
)

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
