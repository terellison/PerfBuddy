# pb_code — C/C++ source heuristics. Library + `pb-code` CLI.
add_library(pb_code src/analyzer.cpp)
target_include_directories(pb_code PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pb_code PUBLIC pb_core)
add_library(PerfBuddy::code ALIAS pb_code)

add_executable(pb-code src/main.cpp)
target_link_libraries(pb-code PRIVATE pb_code)
