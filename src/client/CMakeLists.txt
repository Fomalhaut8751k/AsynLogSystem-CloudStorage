# aux_source_directory(. SRC_LIST)

# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF)

# add_executable(Client  ${SRC_LIST})

# target_link_libraries(Client)

# add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)
aux_source_directory(. SRC_LIST)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置静态链接标志
# set(CMAKE_EXE_LINKER_FLAGS "-static")
set(CMAKE_EXE_LINKER_FLAGS "-static -Wl,--warn-unresolved-symbols -Wl,--warn-once")

add_executable(Client ${SRC_LIST})

# 静态链接所有需要的库
target_link_libraries(Client
    -static
    stdc++fs
    event
)

add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)