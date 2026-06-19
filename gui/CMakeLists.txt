# perfbuddy-gui — Qt6 desktop shell over pb_app.
# Only configured when Qt6 is found (see the top-level CMakeLists).
find_package(Qt6 COMPONENTS Widgets Concurrent REQUIRED)

set(CMAKE_AUTOMOC ON)

add_executable(perfbuddy-gui
  src/main.cpp
  src/MainWindow.cpp
  src/MainWindow.hpp
)
target_link_libraries(perfbuddy-gui PRIVATE
  pb_app pb_core Qt6::Widgets Qt6::Concurrent)
