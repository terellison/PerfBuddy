# pb_memory — allocation-log analysis. Library + `pb-memory` CLI.
add_library(pb_memory src/analyzer.cpp)
target_include_directories(pb_memory PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pb_memory PUBLIC pb_core)
add_library(PerfBuddy::memory ALIAS pb_memory)

add_executable(pb-memory src/main.cpp)
target_link_libraries(pb-memory PRIVATE pb_memory)
