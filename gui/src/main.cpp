#include <QApplication>

#include "MainWindow.hpp"

int main(int argc, char** argv) {
  QApplication app(argc, argv);
  QApplication::setApplicationName("PerfBuddy");
  pb::gui::MainWindow window;
  window.show();
  return app.exec();
}
