add_executable(nodepulse_server
    main.cpp
)

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    target_link_libraries(nodepulse_server
        PRIVATE
            $<LINK_LIBRARY:WHOLE_ARCHIVE,nodepulse_lib>
            Drogon::Drogon
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_libraries(nodepulse_server
        PRIVATE
            -Wl,--whole-archive nodepulse_lib -Wl,--no-whole-archive
            Drogon::Drogon
    )
else()
    target_link_libraries(nodepulse_server
        PRIVATE
            nodepulse_lib
            Drogon::Drogon
    )
endif()

target_compile_features(nodepulse_server PRIVATE cxx_std_20)

if(WARNINGS_AS_ERRORS)
    target_compile_options(nodepulse_server PRIVATE -Wall -Wextra -Wpedantic -Werror)
else()
    target_compile_options(nodepulse_server PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Installation Rules (Phase 15)
include(GNUInstallDirs)
install(TARGETS nodepulse_server
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

