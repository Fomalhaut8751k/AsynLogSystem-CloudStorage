aux_source_directory(. SRC_LIST)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(Client  ${SRC_LIST})

target_link_libraries(Client curl)

add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)
