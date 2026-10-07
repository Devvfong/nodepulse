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

target_link_directories(nodepulse_server
    PRIVATE
        /home/devqii/.local/lib/x86_64-linux-gnu
        /home/devqii/.local/lib
)
target_link_options(nodepulse_server
    PRIVATE
        -L/home/devqii/.local/lib/x86_64-linux-gnu
        -L/home/devqii/.local/lib
        -Wl,-rpath-link,/home/devqii/.local/lib/x86_64-linux-gnu
        -Wl,-rpath,/home/devqii/.local/lib/x86_64-linux-gnu
)

target_compile_features(nodepulse_server PRIVATE cxx_std_20)

if(WARNINGS_AS_ERRORS)
    target_compile_options(nodepulse_server PRIVATE -Wall -Wextra -Wpedantic -Werror)
else()
    target_compile_options(nodepulse_server PRIVATE -Wall -Wextra -Wpedantic)
endif()

