#include "dashboard_view.h"

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QPieSeries>
#include <QtCharts/QValueAxis>

#include <QDateTime>
#include <QDateEdit>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "db.h"

namespace {

QWidget* makeStable(QWidget* widget, const char* id)
{
    widget->setObjectName(QString::fromLatin1(id));
    widget->setAccessibleName(QString::fromLatin1(id));
    return widget;
}

QLineEdit* kpiValue(QWidget* parent, QVBoxLayout* layout, const char* caption,
                    const char* id)
{
    auto* title = new QLabel(QObject::tr(caption), parent);
    auto* field = new QLineEdit(parent);
    field->setReadOnly(true);
    field->setFrame(false);
    // Readout: objectName (== AutomationId) keeps the stable id; the
    // accessibleName mirrors the displayed text (read via UIA Name) and is
    // maintained by applyKpi below - never makeStable() here.
    field->setObjectName(QString::fromLatin1(id));
    auto* value = qobject_cast<QLineEdit*>(field);
    layout->addWidget(title);
    layout->addWidget(value);
    return value;
}

// Every KPI write funnels through here: automation reads the UIA Name
// (== accessibleName) because no attribute key can read a QLineEdit's value.
void applyKpi(QLineEdit* readout, const QString& text)
{
    readout->setText(text);
    readout->setAccessibleName(text);
}

} // namespace

DashboardView::DashboardView(QWidget* parent)
    : QWidget(parent)
{
    makeStable(this, "dashboardTab");

    // -- Filter section (spec 2026-10-20): the same controls and cascade
    // behaviour as the network tab's filterSection; filters apply on Search
    // and Reset restores the unfiltered defaults. Named section so the E2E
    // finds scope into a small UIA subtree (find-performance architecture).
    auto* filterSection = makeStable(new QWidget(this), "dashboardFilterSection");
    auto* filterLayout = new QHBoxLayout(filterSection);
    filterLayout->setContentsMargins(0, 0, 0, 0);

    auto makeCombo = [](QWidget* parent, const char* id, const QStringList& values) {
        auto* combo = new QComboBox(parent);
        combo->setObjectName(QString::fromLatin1(id));
        combo->setAccessibleName(QString::fromLatin1(id));
        combo->addItem(QStringLiteral("All"));
        combo->addItems(values);
        return combo;
    };
    provinceCombo = makeCombo(filterSection, "dashboardProvinceCombo", Db::provinces());
    cityCombo = makeCombo(filterSection, "dashboardCityCombo", Db::citiesFor(QString()));
    technologyCombo =
        makeCombo(filterSection, "dashboardTechnologyCombo", {"2G", "3G", "4G", "5G"});
    operatorCombo = makeCombo(filterSection, "dashboardOperatorCombo",
                              {"MCI", "Irancell", "Rightel", "Shatel Mobile"});

    // Date pair defaults to the data era (the full seeded range), so the
    // default Search equals an unbounded query. Invalid era (empty db)
    // falls back to a wide-open window. Readout contract: objectName keeps
    // the stable id; accessibleName mirrors the displayed text.
    const auto era = Db::dataEra();
    const QDate defaultFrom = era.first.isValid() ? era.first : QDate(2000, 1, 1);
    const QDate defaultTo = era.second.isValid() ? era.second : QDate(2100, 1, 1);
    fromDateEdit = new QDateEdit(defaultFrom, filterSection);
    fromDateEdit->setObjectName(QStringLiteral("dashboardFromDateEdit"));
    toDateEdit = new QDateEdit(defaultTo, filterSection);
    toDateEdit->setObjectName(QStringLiteral("dashboardToDateEdit"));
    for (QDateEdit* edit : {fromDateEdit, toDateEdit}) {
        edit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        edit->setCalendarPopup(false);
        edit->setAccessibleName(edit->text());
    }
    connect(fromDateEdit, &QDateEdit::dateChanged, this, [this](const QDate&) {
        fromDateEdit->setAccessibleName(fromDateEdit->text());
    });
    connect(toDateEdit, &QDateEdit::dateChanged, this, [this](const QDate&) {
        toDateEdit->setAccessibleName(toDateEdit->text());
    });

    auto* searchBtn = new QPushButton(tr("Search"), filterSection);
    searchButton = qobject_cast<QPushButton*>(makeStable(searchBtn, "dashboardSearchButton"));
    searchButton->setDefault(true);
    auto* resetBtn = new QPushButton(tr("Reset"), filterSection);
    resetButton = qobject_cast<QPushButton*>(makeStable(resetBtn, "dashboardResetButton"));

    filterLayout->addWidget(provinceCombo);
    filterLayout->addWidget(cityCombo);
    filterLayout->addWidget(technologyCombo);
    filterLayout->addWidget(operatorCombo);
    filterLayout->addWidget(fromDateEdit);
    filterLayout->addWidget(toDateEdit);
    filterLayout->addWidget(searchButton);
    filterLayout->addWidget(resetButton);
    filterLayout->addStretch(1);

    connect(provinceCombo, &QComboBox::currentIndexChanged,
            this, &DashboardView::onProvinceChanged);
    connect(searchButton, &QPushButton::clicked, this, &DashboardView::onSearch);
    connect(resetButton, &QPushButton::clicked, this, &DashboardView::onReset);

    auto* kpiRow = new QHBoxLayout;
    auto* avgBox = new QVBoxLayout;
    kpiAvgQuality = kpiValue(this, avgBox, "Avg Quality", "kpiAvgQuality");
    kpiRow->addLayout(avgBox);
    auto* cellsBox = new QVBoxLayout;
    kpiCells = kpiValue(this, cellsBox, "Cells", "kpiCells");
    kpiRow->addLayout(cellsBox);
    auto* bestBox = new QVBoxLayout;
    kpiBestOperator = kpiValue(this, bestBox, "Best Operator", "kpiBestOperator");
    kpiRow->addLayout(bestBox);
    auto* worstBox = new QVBoxLayout;
    kpiWorstOperator = kpiValue(this, worstBox, "Worst Operator", "kpiWorstOperator");
    kpiRow->addLayout(worstBox);
    kpiRow->addStretch(1);

    // Persistent axes, created exactly once (chart contract): refresh()
    // replaces the series only and points are set BEFORE the attach.
    auto* chart = new QChart;
    chart->setTitle(tr("Network Quality - last 7 days"));
    chart->legend()->show();
    chart->legend()->setAlignment(Qt::AlignBottom);
    timeAxis = new QDateTimeAxis;
    timeAxis->setFormat(QStringLiteral("yyyy-MM-dd"));
    timeAxis->setTitleText(tr("Day"));
    qualityAxis = new QValueAxis;
    qualityAxis->setRange(0, 100);
    qualityAxis->setTitleText(QStringLiteral("Quality (%)"));
    qualityAxis->setLabelFormat(QStringLiteral("%d"));
    qualityAxis->setTickCount(5);
    chart->addAxis(timeAxis, Qt::AlignBottom);
    chart->addAxis(qualityAxis, Qt::AlignLeft);

    // Plain caption above the chart; no accessibleName override so WAD
    // exposes its TEXT as the UIA Name (readable by the E2E stack).
    trendCaption = new QLabel(this);
    trendCaption->setObjectName(QStringLiteral("trendCaption"));

    trendChart = new QChartView(chart, this);
    makeStable(trendChart, "trendChart");
    trendChart->setRenderHint(QPainter::Antialiasing);

    worstCellsList = qobject_cast<QTableWidget*>(
        makeStable(new QTableWidget(0, 7, this), "worstCellsList"));
    worstCellsList->setHorizontalHeaderLabels(
        {tr("ID"), tr("Province"), tr("City"), tr("Operator"), tr("Technology"),
         tr("Signal Strength"), tr("Timestamp")});
    worstCellsList->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    worstCellsList->setSelectionBehavior(QAbstractItemView::SelectRows);
    worstCellsList->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Operator share pie: caption (Name = its text, slice count readable),
    // chart, and a one-column list whose items mirror every slice - the
    // E2E stack reads the caption + list, never the chart internals.
    operatorShareCaption = new QLabel(this);
    operatorShareCaption->setObjectName(QStringLiteral("operatorShareCaption"));
    auto* pieChart = new QChart;
    pieChart->setTitle(tr("Cells by operator"));
    operatorShareChart = new QChartView(pieChart, this);
    makeStable(operatorShareChart, "operatorShareChart");
    operatorShareChart->setRenderHint(QPainter::Antialiasing);
    operatorShareList = qobject_cast<QListWidget*>(
        makeStable(new QListWidget(this), "operatorShareList"));
    operatorShareList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    operatorShareList->setSelectionMode(QAbstractItemView::NoSelection);

    // Avg-quality-by-technology bar chart: persistent axes created exactly
    // once (network-chart contract); refresh replaces only series and
    // categories, categories BEFORE the attach.
    auto* techChart = new QChart;
    techChart->setTitle(tr("Avg quality by technology"));
    techChart->legend()->setAlignment(Qt::AlignRight);
    techChart->legend()->show();
    techQualityChart = new QChartView(techChart, this);
    makeStable(techQualityChart, "techQualityChart");
    techQualityChart->setRenderHint(QPainter::Antialiasing);
    techAxisX = new QBarCategoryAxis;
    QFont axisFont(QStringLiteral("Segoe UI"), 9);
    techAxisX->setLabelsFont(axisFont);
    techAxisY = new QValueAxis;
    techAxisY->setRange(0, 100);
    techAxisY->setTitleText(QStringLiteral("Quality (%)"));
    techAxisY->setLabelFormat(QStringLiteral("%d"));
    techAxisY->setLabelsFont(axisFont);
    techAxisY->setTickCount(5);
    techChart->addAxis(techAxisX, Qt::AlignBottom);
    techChart->addAxis(techAxisY, Qt::AlignLeft);
    techQualityCaption = new QLabel(this);
    techQualityCaption->setObjectName(QStringLiteral("techQualityCaption"));
    techQualityList = qobject_cast<QListWidget*>(
        makeStable(new QListWidget(this), "techQualityList"));
    techQualityList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    techQualityList->setSelectionMode(QAbstractItemView::NoSelection);

    // Plain caption above the weakest-cells table (Name = its text, no
    // accessibleName override) carrying the row count for the E2E stack -
    // counting UIA DataItems fails on virtualized tables (repo quirk).
    // The distinctive prefix keeps it unique against network status text.
    worstCaption = new QLabel(this);
    worstCaption->setObjectName(QStringLiteral("worstCaption"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(filterSection);
    layout->addLayout(kpiRow);
    layout->addWidget(trendCaption);
    layout->addWidget(trendChart, 1);
    layout->addWidget(worstCaption);
    layout->addWidget(worstCellsList, 1);
    layout->addWidget(operatorShareCaption);
    auto* shareRow = new QHBoxLayout;
    shareRow->addWidget(operatorShareChart, 2);
    shareRow->addWidget(operatorShareList, 1);
    layout->addLayout(shareRow, 1);
    layout->addWidget(techQualityCaption);
    auto* techRow = new QHBoxLayout;
    techRow->addWidget(techQualityChart, 2);
    techRow->addWidget(techQualityList, 1);
    layout->addLayout(techRow, 1);

    refresh();
}

QString DashboardView::comboFilter(const QComboBox* combo) const
{
    const QString value = combo->currentText();
    if (value.isEmpty() || value == QLatin1String("All"))
        return QString();
    return value;
}

void DashboardView::onProvinceChanged(int index)
{
    Q_UNUSED(index);
    // Cascade (network-tab behaviour): the city combo repopulates from the
    // selected province and lands back on All.
    cityCombo->blockSignals(true);
    cityCombo->clear();
    cityCombo->addItem(QStringLiteral("All"));
    cityCombo->addItems(
        comboFilter(provinceCombo).isEmpty() ? Db::citiesFor(QString())
                                             : Db::citiesFor(comboFilter(provinceCombo)));
    cityCombo->setCurrentIndex(0);
    cityCombo->blockSignals(false);
}

void DashboardView::onSearch()
{
    refresh();
}

void DashboardView::onReset()
{
    provinceCombo->blockSignals(true);
    provinceCombo->setCurrentIndex(0);
    provinceCombo->blockSignals(false);
    onProvinceChanged(0); // city back to All + full city list
    technologyCombo->setCurrentIndex(0);
    operatorCombo->setCurrentIndex(0);
    const auto era = Db::dataEra();
    fromDateEdit->setDate(era.first.isValid() ? era.first : QDate(2000, 1, 1));
    toDateEdit->setDate(era.second.isValid() ? era.second : QDate(2100, 1, 1));
    refresh();
}

void DashboardView::refresh()
{
    const QString province = comboFilter(provinceCombo);
    const QString city = comboFilter(cityCombo);
    const QString technology = comboFilter(technologyCombo);
    const QString operatorName = comboFilter(operatorCombo);
    const QDate from = fromDateEdit->date();
    const QDate to = toDateEdit->date();

    applyKpi(kpiAvgQuality,
             QString::number(Db::networkQuality(province, city, technology,
                                                operatorName, from, to),
                             'f', 1)
                 + QStringLiteral("%"));
    applyKpi(kpiCells,
             QString::number(Db::queryFilters(province, city, technology,
                                              operatorName, from, to)
                                 .size())
                 + QStringLiteral(" cells"));

    // Best/worst operator come from the shared aggregate; ties keep the
    // first label in Db's order; zero groups leave the readouts empty.
    applyKpi(kpiBestOperator, QString());
    applyKpi(kpiWorstOperator, QString());
    const auto byOperator =
        Db::avgBy(QStringLiteral("operator"), province, city, technology, operatorName,
                  from, to);
    double bestV = -1e300, worstV = 1e300;
    for (const auto& point : byOperator) {
        if (kpiBestOperator->text() == QString() || point.second > bestV) {
            bestV = point.second;
            applyKpi(kpiBestOperator, point.first);
        }
        if (kpiWorstOperator->text() == QString() || point.second < worstV) {
            worstV = point.second;
            applyKpi(kpiWorstOperator, point.first);
        }
    }

    auto* chart = trendChart->chart();
    chart->removeAllSeries();
    const auto trend = Db::dailyQuality(QString(), QString(), province, city,
                                       technology, operatorName, from, to);
    trendCaption->setText(tr("Daily quality trend - %1 points").arg(trend.size()));
    auto* series = new QLineSeries;
    for (const auto& point : trend) {
        const QDateTime day = QDateTime::fromString(point.first, Qt::ISODate);
        series->append(day.toMSecsSinceEpoch(), point.second);
    }
    if (!trend.isEmpty()) {
        const QDateTime first = QDateTime::fromString(trend.first().first, Qt::ISODate);
        const QDateTime last = QDateTime::fromString(trend.last().first, Qt::ISODate);
        timeAxis->setRange(first, last);
        qualityAxis->setRange(0, 100);
    }
    // Points must exist BEFORE the persistent axis syncs to the series:
    // an axis attached to an empty series auto-generates index placeholders.
    chart->addSeries(series);
    series->attachAxis(timeAxis);
    series->attachAxis(qualityAxis);

    const auto worst =
        Db::worstCells(8, province, city, technology, operatorName, from, to);
    worstCaption->setText(
        tr("Weakest cells - %1 rows").arg(worst.size()));
    worstCellsList->setRowCount(0);
    for (const auto& row : worst) {
        const int r = worstCellsList->rowCount();
        worstCellsList->insertRow(r);
        for (int c = 0; c < row.size() && c < worstCellsList->columnCount(); ++c)
            worstCellsList->setItem(r, c, new QTableWidgetItem(row.at(c)));
    }

    // Operator share pie: slice per countBy label; labels visible with an
    // explicit black brush (the theme picks near-white on colored slices).
    // The list mirrors the slices in the pinned format.
    const auto share =
        Db::countBy(QStringLiteral("operator"), province, city, technology, operatorName,
                    from, to);
    double shareTotal = 0;
    for (const auto& slice : share)
        shareTotal += slice.second;
    operatorShareCaption->setText(
        tr("Cells by operator - %1 slices").arg(share.size()));
    operatorShareList->clear();
    auto* pie = new QPieSeries;
    for (const auto& slice : share) {
        const double pct = shareTotal > 0 ? 100.0 * slice.second / shareTotal : 0.0;
        operatorShareList->addItem(QStringLiteral("%1: %2 cells (%3%)")
                                       .arg(slice.first,
                                            QString::number(slice.second),
                                            QString::number(pct, 'f', 1)));
        auto* pieSlice = pie->append(slice.first, slice.second);
        pieSlice->setLabel(
            QStringLiteral("%1 %2%").arg(slice.first, QString::number(pct, 'f', 1)));
        pieSlice->setLabelVisible(true);
        pieSlice->setLabelBrush(Qt::black);
    }
    auto* pieChartPtr = operatorShareChart->chart();
    pieChartPtr->removeAllSeries();
    pieChartPtr->addSeries(pie);

    // Avg quality by technology: bars encode the shared 0..100 quality
    // metric (never raw dBm), labels @value/precision 3 with an explicit
    // black brush; categories set BEFORE the series attaches.
    const auto techAvgs =
        Db::avgBy(QStringLiteral("technology"), province, city, technology, operatorName,
                  from, to);
    techQualityCaption->setText(
        tr("Avg quality by technology - %1 groups").arg(techAvgs.size()));
    techQualityList->clear();
    auto* techSeries = new QBarSeries;
    techSeries->setLabelsVisible(true);
    techSeries->setLabelsPosition(QAbstractBarSeries::LabelsOutsideEnd);
    techSeries->setLabelsFormat(QStringLiteral("@value"));
    techSeries->setLabelsPrecision(3);
    auto* barSet = new QBarSet(tr("Quality (%)"));
    barSet->setLabelBrush(Qt::black);
    QStringList categories;
    for (const auto& group : techAvgs) {
        categories << group.first;
        *barSet << Db::qualityFrom(group.second);
        techQualityList->addItem(QStringLiteral("%1: %2%")
                                     .arg(group.first,
                                          QString::number(Db::qualityFrom(group.second),
                                                          'f', 1)));
    }
    techAxisX->clear();
    techAxisX->append(categories);
    techSeries->append(barSet);
    auto* techChartPtr = techQualityChart->chart();
    techChartPtr->removeAllSeries();
    techChartPtr->addSeries(techSeries);
    techSeries->attachAxis(techAxisX);
    techSeries->attachAxis(techAxisY);
}
