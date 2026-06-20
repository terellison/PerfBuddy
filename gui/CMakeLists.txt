# perfbuddy-gui — Qt6 desktop shell over pb_app.
# Only configured when Qt6 is found (see the top-level CMakeLists).
find_package(Qt6 COMPONENTS Widgets Concurrent REQUIRED)

set(CMAKE_AUTOMOC ON)

# MACOSX_BUNDLE makes this a proper .app on macOS (required by macdeployqt and
# the natural form to ship); it is ignored on other platforms. WIN32 keeps the
# Windows build a GUI app with no extra console window.
add_executable(perfbuddy-gui WIN32 MACOSX_BUNDLE
  src/main.cpp
  src/MainWindow.cpp
  src/MainWindow.hpp
)
target_link_libraries(perfbuddy-gui PRIVATE
  pb_app pb_core Qt6::Widgets Qt6::Concurrent)

# Install the GUI: a .app bundle on macOS, a plain executable elsewhere. The
# release workflow runs the platform Qt deploy tool over the installed result.
install(TARGETS perfbuddy-gui
  BUNDLE  DESTINATION .
  RUNTIME DESTINATION bin)
