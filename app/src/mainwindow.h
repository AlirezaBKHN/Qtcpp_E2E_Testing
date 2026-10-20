#pragma once

#include <QComboBox>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>

class QAbstractAxis;
class QBarCategoryAxis;
class QChartView;
class QValueAxis;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString& user, QWidget* parent = nullptr);

    // The signed-in username shown in the top bar (Task 4 wiring).
    QString currentUser() const { return currentUser_; }

    // True when the window was closed without a logout (X button) - the
    // sign-in loop in main.cpp exits the app in that case; logout instead
    // reopens SignInDialog.
    bool shouldExit() const { return shouldExit_; }

private slots:
    void onSearch();
    void onRefresh();
    void onReset();
    void onExport();
    void onProvinceChanged(int index);
    void onLogout();

private:
    void refreshChart();
    QComboBox* provinceCombo = nullptr;
    QComboBox* cityCombo = nullptr;
    QComboBox* technologyCombo = nullptr;
    QComboBox* operatorCombo = nullptr;
    QComboBox* chartGroupBy = nullptr;

    QPushButton* searchButton = nullptr;
    QPushButton* refreshButton = nullptr;
    QPushButton* resetButton = nullptr;
    QPushButton* exportButton = nullptr;

    QTableWidget* signalTable = nullptr;
    QChartView* signalChart = nullptr;
    QLineEdit* currentUserLabel = nullptr; // readout (value "user: <name>")
    QTabWidget* mainTabs = nullptr;

    QBarCategoryAxis* axisX = nullptr;
    QValueAxis* axisY = nullptr;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    bool loggingOut_ = false;
    bool shouldExit_ = false;
    QString currentUser_;
};
