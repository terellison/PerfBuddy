#pragma once
//
// PerfBuddy GUI — a thin Qt6 desktop shell over pb_app.
//
// It owns NO analysis logic. It collects a Target from the user, calls the very
// same pb::app::run_analysis() the CLI uses (on a worker thread), and renders
// the findings grouped by module and severity.
//
#include <QFutureWatcher>
#include <QMainWindow>
#include <vector>

#include "pb_core/types.hpp"

class QLineEdit;
class QComboBox;
class QCheckBox;
class QTreeWidget;
class QLabel;
class QPushButton;

namespace pb::gui {

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);

 private slots:
  void browseExecutable();
  void browseSource();
  void browseData();
  void runAnalysis();
  void onAnalysisFinished();
  void exportJson();

 private:
  pb::Target buildTarget() const;
  std::vector<std::string> selectedModules() const;
  void populateTree(const pb::Report& report);
  void setBusy(bool busy);

  QLineEdit* exe_edit_ = nullptr;
  QLineEdit* src_edit_ = nullptr;
  QLineEdit* data_edit_ = nullptr;
  QComboBox* engine_combo_ = nullptr;
  std::vector<QCheckBox*> module_checks_;
  QPushButton* run_btn_ = nullptr;
  QPushButton* export_btn_ = nullptr;
  QLabel* summary_label_ = nullptr;
  QTreeWidget* tree_ = nullptr;

  QFutureWatcher<pb::Report> watcher_;
  pb::Report last_report_;
};

}  // namespace pb::gui
