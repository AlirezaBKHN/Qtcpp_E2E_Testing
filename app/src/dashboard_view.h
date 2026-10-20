#pragma once

#include <QChartView>
#include <QComboBox>
#include <QDateEdit>
#include <QTableWidget>
#include <QWidget>

class QDateTimeAxis;
class QValueAxis;
class QBarCategoryAxis;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

// Dashboard tab: filter section (province/city/technology/operator + date
// from-to, applied on Search), network KPI row, quality trend line chart on
// a persistent time axis, weakest-cells table. All values come from the Db
// aggregates - no UI-side recomputation (chart contract: persistent axes,
// points before attach, see plan Global Constraints). The filter state
// lives in the widgets; refresh() reads them and every widget on the tab
// reflects the same filtered query.
class DashboardView : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardView(QWidget* parent = nullptr);

private slots:
    void onProvinceChanged(int index);
    void onSearch();
    void onReset();

private:
    void refresh();
    // Widget filter state as Db arguments: "All" = unbounded column; the
    // date edits always carry a valid day (era defaults) so the default
    // Search equals the whole seeded range.
    QString comboFilter(const QComboBox* combo) const;

    // Filter section: same combo populations as the network tab (All
    // first); cascaded city combo; era-defaulted date pair. Readout
    // contract on the date pair: accessibleName mirrors the displayed date.
    QComboBox* provinceCombo = nullptr;
    QComboBox* cityCombo = nullptr;
    QComboBox* technologyCombo = nullptr;
    QComboBox* operatorCombo = nullptr;
    QDateEdit* fromDateEdit = nullptr;
    QDateEdit* toDateEdit = nullptr;
    QPushButton* searchButton = nullptr;
    QPushButton* resetButton = nullptr;

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

    // Operator share pie + its readable surfaces: caption carries the
    // slice count (Name = text), the one-column list mirrors each slice's
    // pinned "label: N cells (P.P%)" text - WAD cannot read chart series.
    QLabel* operatorShareCaption = nullptr;
    QChartView* operatorShareChart = nullptr;
    QListWidget* operatorShareList = nullptr;

    // Avg-quality-by-technology bar chart (the network chart's pinned
    // contract: persistent axes, categories before attach, @value labels,
    // black brush) + its readable caption/list pair.
    QLabel* techQualityCaption = nullptr;
    QChartView* techQualityChart = nullptr;
    QListWidget* techQualityList = nullptr;
    QBarCategoryAxis* techAxisX = nullptr;
    QValueAxis* techAxisY = nullptr;
};