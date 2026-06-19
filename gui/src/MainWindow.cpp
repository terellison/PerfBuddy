#include "MainWindow.hpp"

#include <QtConcurrent>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextStream>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "pb_app/app.hpp"
#include "pb_core/report.hpp"

namespace pb::gui {

namespace {

QString sevName(pb::Severity s) {
  switch (s) {
    case pb::Severity::Info: return "INFO";
    case pb::Severity::Low: return "LOW";
    case pb::Severity::Medium: return "MEDIUM";
    case pb::Severity::High: return "HIGH";
    case pb::Severity::Critical: return "CRITICAL";
  }
  return "INFO";
}

QColor sevColor(pb::Severity s) {
  switch (s) {
    case pb::Severity::Info: return QColor(140, 140, 140);
    case pb::Severity::Low: return QColor(120, 160, 60);
    case pb::Severity::Medium: return QColor(220, 160, 30);
    case pb::Severity::High: return QColor(220, 90, 40);
    case pb::Severity::Critical: return QColor(200, 40, 40);
  }
  return QColor(140, 140, 140);
}

// A row that picks a path, with a Browse button.
QWidget* makePathRow(const QString& label, QLineEdit*& edit, QPushButton*& btn) {
  auto* w = new QWidget;
  auto* h = new QHBoxLayout(w);
  h->setContentsMargins(0, 0, 0, 0);
  auto* l = new QLabel(label);
  l->setMinimumWidth(90);
  edit = new QLineEdit;
  btn = new QPushButton("Browse…");
  h->addWidget(l);
  h->addWidget(edit, 1);
  h->addWidget(btn);
  return w;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  setWindowTitle("PerfBuddy — Game Performance Analyzer");
  resize(1000, 720);

  auto* central = new QWidget;
  auto* root = new QVBoxLayout(central);

  // ---- Inputs ----
  auto* inputs = new QGroupBox("Target");
  auto* iv = new QVBoxLayout(inputs);
  QPushButton *exeBtn, *srcBtn, *dataBtn;
  iv->addWidget(makePathRow("Executable", exe_edit_, exeBtn));
  iv->addWidget(makePathRow("Source dir", src_edit_, srcBtn));
  iv->addWidget(makePathRow("Data dir", data_edit_, dataBtn));

  auto* engineRow = new QWidget;
  auto* eh = new QHBoxLayout(engineRow);
  eh->setContentsMargins(0, 0, 0, 0);
  auto* el = new QLabel("Engine");
  el->setMinimumWidth(90);
  engine_combo_ = new QComboBox;
  engine_combo_->addItems({"auto-detect", "native", "unreal"});
  eh->addWidget(el);
  eh->addWidget(engine_combo_);
  eh->addStretch(1);
  iv->addWidget(engineRow);
  root->addWidget(inputs);

  // ---- Module selection ----
  auto* modBox = new QGroupBox("Modules");
  auto* mh = new QHBoxLayout(modBox);
  for (const auto& m : pb::app::list_modules()) {
    auto* cb = new QCheckBox(QString::fromStdString(m.name));
    cb->setChecked(true);
    cb->setToolTip(QString::fromStdString(m.description));
    cb->setProperty("module", QString::fromStdString(m.name));
    module_checks_.push_back(cb);
    mh->addWidget(cb);
  }
  mh->addStretch(1);
  root->addWidget(modBox);

  // ---- Actions ----
  auto* actions = new QHBoxLayout;
  run_btn_ = new QPushButton("Run analysis");
  run_btn_->setDefault(true);
  export_btn_ = new QPushButton("Export JSON…");
  export_btn_->setEnabled(false);
  summary_label_ = new QLabel("Pick a target and run.");
  actions->addWidget(run_btn_);
  actions->addWidget(export_btn_);
  actions->addStretch(1);
  actions->addWidget(summary_label_);
  root->addLayout(actions);

  // ---- Results ----
  tree_ = new QTreeWidget;
  tree_->setColumnCount(3);
  tree_->setHeaderLabels({"Severity / Module", "Finding", "Fix"});
  tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
  tree_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
  tree_->header()->setSectionResizeMode(2, QHeaderView::Stretch);
  root->addWidget(tree_, 1);

  setCentralWidget(central);

  connect(exeBtn, &QPushButton::clicked, this, &MainWindow::browseExecutable);
  connect(srcBtn, &QPushButton::clicked, this, &MainWindow::browseSource);
  connect(dataBtn, &QPushButton::clicked, this, &MainWindow::browseData);
  connect(run_btn_, &QPushButton::clicked, this, &MainWindow::runAnalysis);
  connect(export_btn_, &QPushButton::clicked, this, &MainWindow::exportJson);
  connect(&watcher_, &QFutureWatcher<pb::Report>::finished, this,
          &MainWindow::onAnalysisFinished);
}

void MainWindow::browseExecutable() {
  QString f = QFileDialog::getOpenFileName(this, "Select game executable");
  if (!f.isEmpty()) exe_edit_->setText(f);
}
void MainWindow::browseSource() {
  QString d = QFileDialog::getExistingDirectory(this, "Select source directory");
  if (!d.isEmpty()) src_edit_->setText(d);
}
void MainWindow::browseData() {
  QString d = QFileDialog::getExistingDirectory(this, "Select data directory");
  if (!d.isEmpty()) data_edit_->setText(d);
}

pb::Target MainWindow::buildTarget() const {
  pb::Target t;
  if (!exe_edit_->text().isEmpty()) t.executable = exe_edit_->text().toStdString();
  if (!src_edit_->text().isEmpty()) t.source_dir = src_edit_->text().toStdString();
  if (!data_edit_->text().isEmpty()) t.data_dir = data_edit_->text().toStdString();
  QString eng = engine_combo_->currentText();
  if (eng == "native") t.engine = pb::Engine::Native;
  else if (eng == "unreal") t.engine = pb::Engine::Unreal;
  t.label = t.executable.value_or(t.source_dir.value_or(t.data_dir.value_or("target")));
  return t;
}

std::vector<std::string> MainWindow::selectedModules() const {
  std::vector<std::string> only;
  for (auto* cb : module_checks_)
    if (cb->isChecked()) only.push_back(cb->property("module").toString().toStdString());
  return only;
}

void MainWindow::setBusy(bool busy) {
  run_btn_->setEnabled(!busy);
  run_btn_->setText(busy ? "Analyzing…" : "Run analysis");
}

void MainWindow::runAnalysis() {
  pb::Target t = buildTarget();
  if (!t.executable && !t.source_dir && !t.data_dir) {
    QMessageBox::warning(this, "PerfBuddy",
                         "Choose at least one of: executable, source dir, data dir.");
    return;
  }
  auto only = selectedModules();
  if (only.empty()) {
    QMessageBox::warning(this, "PerfBuddy", "Select at least one module.");
    return;
  }
  setBusy(true);
  summary_label_->setText("Running…");
  // Same call the CLI makes — off the UI thread so the window stays responsive.
  watcher_.setFuture(QtConcurrent::run([t, only] {
    return pb::app::run_analysis(t, only, true);
  }));
}

void MainWindow::onAnalysisFinished() {
  last_report_ = watcher_.result();
  populateTree(last_report_);
  setBusy(false);
  export_btn_->setEnabled(!last_report_.modules.empty());
}

void MainWindow::populateTree(const pb::Report& report) {
  tree_->clear();
  int total = 0, high = 0;
  for (const auto& m : report.modules) {
    auto* top = new QTreeWidgetItem(tree_);
    top->setText(0, QString::fromStdString(m.module));
    top->setText(1, QString("%1 findings").arg(m.findings.size()));
    if (m.error)
      top->setText(2, "ERROR: " + QString::fromStdString(*m.error));
    top->setExpanded(true);

    for (const auto& f : m.findings) {
      ++total;
      if (static_cast<int>(f.severity) >= static_cast<int>(pb::Severity::High)) ++high;
      auto* child = new QTreeWidgetItem(top);
      child->setText(0, sevName(f.severity));
      child->setForeground(0, sevColor(f.severity));
      QString title = QString::fromStdString(f.title);
      if (f.location) title += "  @ " + QString::fromStdString(f.location->display());
      child->setText(1, title);
      child->setText(2, QString::fromStdString(f.remediation));
      QString tip = QString::fromStdString(f.description);
      if (!f.impact.empty())
        tip += "\n\nImpact: " + QString::fromStdString(f.impact);
      child->setToolTip(1, tip);
    }
  }
  summary_label_->setText(
      QString("%1 module(s), %2 finding(s), %3 high/critical")
          .arg(report.modules.size()).arg(total).arg(high));
  if (report.modules.empty())
    summary_label_->setText("No module could analyze the given target.");
}

void MainWindow::exportJson() {
  if (last_report_.modules.empty()) return;
  QString f = QFileDialog::getSaveFileName(this, "Export report", "perfbuddy-report.json",
                                           "JSON (*.json)");
  if (f.isEmpty()) return;
  QFile out(f);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QMessageBox::warning(this, "PerfBuddy", "Could not write file.");
    return;
  }
  QTextStream(&out) << QString::fromStdString(last_report_.dump(2)) << "\n";
  summary_label_->setText("Exported " + f);
}

}  // namespace pb::gui
