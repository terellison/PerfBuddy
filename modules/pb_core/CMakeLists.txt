# pb_core — shared types, Analyzer interface, JSON, and helpers.
# Library only; depends on nothing but the C++17 standard library.
add_library(pb_core
  src/json.cpp
  src/types.cpp
  src/util.cpp
  src/report.cpp
  src/ruleset.cpp
)
target_include_directories(pb_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(pb_core PUBLIC cxx_std_17)
add_library(PerfBuddy::core ALIAS pb_core)
