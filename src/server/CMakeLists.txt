aux_source_directory(. SRC_LIST)

add_executable(CloudStorage  ${SRC_LIST})

target_link_libraries(CloudStorage event event_pthreads pthread)

add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)
