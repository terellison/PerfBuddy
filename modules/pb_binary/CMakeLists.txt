# pb_binary — executable size & layout. Library + standalone `pb-binary` CLI.
add_library(pb_binary src/analyzer.cpp)
target_include_directories(pb_binary PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pb_binary PUBLIC pb_core)
add_library(PerfBuddy::binary ALIAS pb_binary)

add_executable(pb-binary src/main.cpp)
target_link_libraries(pb-binary PRIVATE pb_binary)
