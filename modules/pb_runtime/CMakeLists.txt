# pb_runtime — frame-time / profiler hot spots. Library + `pb-runtime` CLI.
add_library(pb_runtime src/analyzer.cpp)
target_include_directories(pb_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pb_runtime PUBLIC pb_core)
add_library(PerfBuddy::runtime ALIAS pb_runtime)

add_executable(pb-runtime src/main.cpp)
target_link_libraries(pb-runtime PRIVATE pb_runtime)
