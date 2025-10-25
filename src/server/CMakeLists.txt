aux_source_directory(. SRC_LIST)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Service ${SRC_LIST})

target_link_libraries(Service event event_pthreads pthread stdc++fs jsoncpp bundle)

# 确保 RPATH 设置
set_target_properties(Service PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN"
    SKIP_BUILD_RPATH FALSE
)

add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)