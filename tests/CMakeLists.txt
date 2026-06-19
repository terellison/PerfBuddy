# Unit tests — header-only harness (pb_test.hpp), no third-party dependencies.
# One binary per module, registered with ctest.

function(pb_add_test name)
  add_executable(${name} ${name}.cpp)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(${name} PRIVATE ${ARGN})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

pb_add_test(pb_core_test pb_core)
pb_add_test(pb_code_test pb_code)
pb_add_test(pb_unreal_test pb_unreal)
pb_add_test(pb_app_test pb_app)
