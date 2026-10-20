#pragma once

#include <QChartView>
#include <QTableWidget>
#include <QWidget>

class QDateTimeAxis;
class QValueAxis;
class QLabel;

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

    QLabel* kpiAvgQuality = nullptr;
    QLabel* kpiCells = nullptr;
    QLabel* kpiBestOperator = nullptr;
    QLabel* kpiWorstOperator = nullptr;
    QChartView* trendChart = nullptr;
    QTableWidget* worstCellsList = nullptr;
    QDateTimeAxis* timeAxis = nullptr;
    QValueAxis* qualityAxis = nullptr;
};
