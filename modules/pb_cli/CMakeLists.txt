# pb_cli — the `perfbuddy` orchestrator binary. Pure composition over pb_app.
add_executable(perfbuddy src/main.cpp)
target_link_libraries(perfbuddy PRIVATE pb_app pb_core)
