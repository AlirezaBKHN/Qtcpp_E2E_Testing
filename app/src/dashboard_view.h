#pragma once

#include <QChartView>
#include <QTableWidget>
#include <QWidget>

class QDateTimeAxis;
class QValueAxis;
class QLabel;
class QLineEdit;

// Dashboard tab: network KPI row, 7-day quality trend line chart on a
// persistent time axis, weakest-cells table. All values come from the Db
// aggregates - no UI-side recomputation (chart contract: persistent axes,
// categories before attach, see plan Global Constraints).
class DashboardView : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardView(QWidget* parent = nullptr);

private:
    void refresh();

    // KPI readouts: read-only frameless LineEdits whose objectName (==
    // AutomationId) keeps the stable id and whose accessibleName mirrors the
    // displayed text (automation reads the UIA Name; no attribute key can
    // read a QLineEdit's value).
    QLineEdit* kpiAvgQuality = nullptr;
    QLineEdit* kpiCells = nullptr;
    QLineEdit* kpiBestOperator = nullptr;
    QLineEdit* kpiWorstOperator = nullptr;
    // Plain-text trend caption (Name = its text; no accessibleName override)
    // carrying the bucket count so E2E can read trend_points().
    QLabel* trendCaption = nullptr;
    // Same for the weakest-cells table row count (virtualization-safe read).
    QLabel* worstCaption = nullptr;
    QChartView* trendChart = nullptr;
    QTableWidget* worstCellsList = nullptr;
    QDateTimeAxis* timeAxis = nullptr;
    QValueAxis* qualityAxis = nullptr;
};
