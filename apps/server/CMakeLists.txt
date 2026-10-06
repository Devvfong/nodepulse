add_executable(nodepulse_server
    main.cpp
)

target_link_libraries(nodepulse_server
    PRIVATE
        nodepulse_lib
        Drogon::Drogon
)

target_compile_features(nodepulse_server PRIVATE cxx_std_20)

if(WARNINGS_AS_ERRORS)
    target_compile_options(nodepulse_server PRIVATE -Wall -Wextra -Wpedantic -Werror)
else()
    target_compile_options(nodepulse_server PRIVATE -Wall -Wextra -Wpedantic)
endif()

