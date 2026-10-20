#pragma once

#include <QComboBox>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>

#include "db.h"

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
    void onPageSizeChanged(int index);
    void onNextPage();
    void onPrevPage();

private:
    void refreshChart();
    void populatePage();
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

    // Pager (spec 2026-10-20): one page of m_rows in the table at a time.
    QComboBox* pageSizeCombo = nullptr;
    QPushButton* prevPageButton = nullptr;
    QPushButton* nextPageButton = nullptr;
    QLineEdit* pageLabel = nullptr; // readout (value "Page k of n")
    FilterBindings m_rows; // db.h file-level alias (QVector<QVariantList>)
    int m_page = 1;
    int m_pageSize = 200;

    QBarCategoryAxis* axisX = nullptr;
    QValueAxis* axisY = nullptr;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    bool loggingOut_ = false;
    bool shouldExit_ = false;
    QString currentUser_;
};
