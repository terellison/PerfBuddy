cmake_minimum_required(VERSION 3.16)
project(PerfBuddy VERSION 0.1.0 LANGUAGES CXX)

# ---------------------------------------------------------------------------
# PerfBuddy — a suite of tools that analyze a game's executable and codebase
# to surface concrete performance and quality improvements.
#
# Layout (separation of concerns):
#   modules/pb_core      shared types + Analyzer interface + JSON   (library)
#   modules/pb_binary    executable size / layout                   (lib + CLI)
#   modules/pb_runtime   frame-time / profiler hot spots            (lib + CLI)
#   modules/pb_memory    allocation log analysis                    (lib + CLI)
#   modules/pb_code      C/C++ source heuristics                    (lib + CLI)
#   modules/pb_unreal    Unreal Engine project analysis             (lib + CLI)
#   modules/pb_app       composition: registry + orchestrator       (library)
#   modules/pb_cli       `perfbuddy` orchestrator binary            (CLI)
#   gui/                 Qt6 desktop GUI over pb_app    (optional CLI target)
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(PERFBUDDY_BUILD_GUI "Build the Qt6 desktop GUI (requires Qt6)" ON)
option(PERFBUDDY_BUILD_TESTS "Build unit tests" ON)

enable_testing()

# Core + analysis modules (no third-party dependencies).
add_subdirectory(modules/pb_core)
add_subdirectory(modules/pb_binary)
add_subdirectory(modules/pb_runtime)
add_subdirectory(modules/pb_memory)
add_subdirectory(modules/pb_code)
add_subdirectory(modules/pb_unreal)

# Composition layer + CLI orchestrator.
add_subdirectory(modules/pb_app)
add_subdirectory(modules/pb_cli)

# Optional Qt GUI. Never blocks the rest of the build.
if(PERFBUDDY_BUILD_GUI)
  find_package(Qt6 COMPONENTS Widgets QUIET)
  if(Qt6_FOUND)
    add_subdirectory(gui)
  else()
    message(STATUS "Qt6 not found — skipping the GUI target (everything else still builds). "
                   "Install Qt6 and re-run CMake to enable it.")
  endif()
endif()

if(PERFBUDDY_BUILD_TESTS)
  add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Install rules — `cmake --install <build> --prefix <dir>` lays the CLI tools
# out under <dir>/bin. This is what the release workflow packages into the
# downloadable archives. The GUI installs itself (see gui/CMakeLists.txt) when
# Qt is present; the release workflow then runs the platform Qt deploy tool over
# the installed result to make it self-contained.
# ---------------------------------------------------------------------------
install(TARGETS
          perfbuddy
          pb-binary
          pb-runtime
          pb-memory
          pb-code
          pb-unreal
        RUNTIME DESTINATION bin)

message(STATUS "PerfBuddy configured. Binaries: pb-binary pb-runtime pb-memory pb-code pb-unreal perfbuddy")
