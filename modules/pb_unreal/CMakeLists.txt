# pb_unreal — Unreal Engine analysis. Library + `pb-unreal` CLI.
add_library(pb_unreal src/analyzer.cpp)
target_include_directories(pb_unreal PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pb_unreal PUBLIC pb_core)
add_library(PerfBuddy::unreal ALIAS pb_unreal)

add_executable(pb-unreal src/main.cpp)
target_link_libraries(pb-unreal PRIVATE pb_unreal)
