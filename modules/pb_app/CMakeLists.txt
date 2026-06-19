# pb_app — composition layer: aggregates all analyzers and orchestrates runs.
# This is the only library that depends on every module.
add_library(pb_app src/app.cpp)
target_include_directories(pb_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(pb_app PUBLIC
  pb_core pb_binary pb_runtime pb_memory pb_code pb_unreal)

find_package(Threads REQUIRED)
target_link_libraries(pb_app PUBLIC Threads::Threads)

add_library(PerfBuddy::app ALIAS pb_app)
