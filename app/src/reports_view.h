#pragma once

#include <QComboBox>
#include <QChartView>
#include <QTableWidget>
#include <QWidget>

class QDateTimeAxis;
class QValueAxis;

// Reports tab: group-by summary (Label / Rows / Avg Quality), a per-label
// drill-down daily-trend line chart, and a CSV export following the same
// silent-writer pattern as the network export (reports/export/
// report_<timestamp>.csv). Confirmation travels through MainWindow's
// status bar via the exported() signal - exact copy "Exported: <path>".
class ReportsView : public QWidget
{
    Q_OBJECT

public:
    explicit ReportsView(QWidget* parent = nullptr);

signals:
    void exported(const QString& path, bool ok);

private slots:
    void onGroupChanged(const QString& group);
    void onSelectionChanged(int currentRow);
    void onExport();

private:
    void refreshSummary();
    void refreshDrill(int currentRow);

    QComboBox* reportGroupBy = nullptr;
    QTableWidget* reportTable = nullptr;
    QChartView* dailyTrendChart = nullptr;
    QDateTimeAxis* timeAxis = nullptr;
    QValueAxis* qualityAxis = nullptr;
};
