#pragma once

#include <QComboBox>
#include <QChartView>
#include <QDateEdit>
#include <QTableWidget>
#include <QWidget>

class QDateTimeAxis;
class QValueAxis;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

// Reports tab: filter section (same rules as the dashboard's), group-by
// summary (Label / Rows / Avg Quality), a group-share pie readable through
// caption + list, a per-label drill-down daily-trend line chart, and a CSV
// export following the same silent-writer pattern as the network export
// (reports/export/report_<timestamp>.csv). Confirmation travels through
// MainWindow's status bar via the exported() signal - exact copy
// "Exported: <path>". The export always writes the CURRENT (filtered)
// summary table.
class ReportsView : public QWidget
{
    Q_OBJECT

public:
    explicit ReportsView(QWidget* parent = nullptr);

signals:
    void exported(const QString& path, bool ok);

private slots:
    void onProvinceChanged(int index);
    void onSearch();
    void onReset();
    void onGroupChanged(const QString& group);
    void onSelectionChanged(int currentRow);
    void onExport();

private:
    // Widget filter state as Db arguments ("All" = unbounded column; the
    // date pair always carries a valid era-defaulted day).
    struct CurrentFilters
    {
        QString province;
        QString city;
        QString technology;
        QString operatorName;
        QDate from;
        QDate to;
    };
    CurrentFilters currentFilters() const;
    void refreshSummary();
    void refreshDrill(int currentRow);

    // Filter section (dashboard rules, reports* ids; readout contract on
    // the date pair: accessibleName mirrors the displayed date).
    QComboBox* provinceCombo = nullptr;
    QComboBox* cityCombo = nullptr;
    QComboBox* technologyCombo = nullptr;
    QComboBox* operatorCombo = nullptr;
    QDateEdit* fromDateEdit = nullptr;
    QDateEdit* toDateEdit = nullptr;
    QPushButton* searchButton = nullptr;
    QPushButton* resetButton = nullptr;

    QComboBox* reportGroupBy = nullptr;
    QTableWidget* reportTable = nullptr;
    QChartView* dailyTrendChart = nullptr;
    QDateTimeAxis* timeAxis = nullptr;
    QValueAxis* qualityAxis = nullptr;

    // Group share pie + its readable surfaces (caption carries the slice
    // count; list items mirror the summary Rows column, pinned format).
    QLabel* groupShareCaption = nullptr;
    QChartView* groupShareChart = nullptr;
    QListWidget* groupShareList = nullptr;
};
