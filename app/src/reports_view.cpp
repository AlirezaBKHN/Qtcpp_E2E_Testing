#include "reports_view.h"

#include <QtCharts/QChart>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QPieSeries>
#include <QtCharts/QValueAxis>

#include <QCoreApplication>
#include <QDateEdit>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
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

QString groupColumnForGroupText(const QString& group)
{
    if (group == QLatin1String("Operator"))
        return QStringLiteral("operator");
    if (group == QLatin1String("Technology"))
        return QStringLiteral("technology");
    return QStringLiteral("province"); // Province is the default group
}

} // namespace

ReportsView::ReportsView(QWidget* parent)
    : QWidget(parent)
{
    makeStable(this, "reportsTab");

    // -- Filter section (spec 2026-10-20): the dashboard section's rules
    // with reports* ids; filters apply on Search, Reset restores All + era.
    auto* filterSection = makeStable(new QWidget(this), "reportsFilterSection");
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
    provinceCombo = makeCombo(filterSection, "reportsProvinceCombo", Db::provinces());
    cityCombo = makeCombo(filterSection, "reportsCityCombo", Db::citiesFor(QString()));
    technologyCombo =
        makeCombo(filterSection, "reportsTechnologyCombo", {"2G", "3G", "4G", "5G"});
    operatorCombo = makeCombo(filterSection, "reportsOperatorCombo",
                              {"MCI", "Irancell", "Rightel", "Shatel Mobile"});

    const auto era = Db::dataEra();
    const QDate defaultFrom = era.first.isValid() ? era.first : QDate(2000, 1, 1);
    const QDate defaultTo = era.second.isValid() ? era.second : QDate(2100, 1, 1);
    fromDateEdit = new QDateEdit(defaultFrom, filterSection);
    fromDateEdit->setObjectName(QStringLiteral("reportsFromDateEdit"));
    toDateEdit = new QDateEdit(defaultTo, filterSection);
    toDateEdit->setObjectName(QStringLiteral("reportsToDateEdit"));
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
    searchButton = qobject_cast<QPushButton*>(makeStable(searchBtn, "reportsSearchButton"));
    searchButton->setDefault(true);
    auto* resetBtn = new QPushButton(tr("Reset"), filterSection);
    resetButton = qobject_cast<QPushButton*>(makeStable(resetBtn, "reportsResetButton"));

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
            this, &ReportsView::onProvinceChanged);
    connect(searchButton, &QPushButton::clicked, this, &ReportsView::onSearch);
    connect(resetButton, &QPushButton::clicked, this, &ReportsView::onReset);

    auto* groupLabel = new QLabel(tr("Group by"), this);
    auto* groupCombo = new QComboBox(this);
    reportGroupBy = qobject_cast<QComboBox*>(makeStable(groupCombo, "reportGroupBy"));
    reportGroupBy->addItems({tr("Province"), tr("Operator"), tr("Technology")});

    reportTable = qobject_cast<QTableWidget*>(
        makeStable(new QTableWidget(0, 3, this), "reportTable"));
    reportTable->setHorizontalHeaderLabels(
        {tr("Label"), tr("Rows"), tr("Avg Quality")});
    reportTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    reportTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    reportTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    reportTable->setSortingEnabled(true);

    auto* exportBtn = new QPushButton(tr("Export CSV"), this);
    exportBtn = qobject_cast<QPushButton*>(makeStable(exportBtn, "exportReportButton"));

    // Persistent drill chart axes (chart contract, same as dashboard):
    // created once; series replaced per selection, points before attach.
    auto* chart = new QChart;
    chart->setTitle(tr("Daily Quality"));
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

    dailyTrendChart = new QChartView(chart, this);
    makeStable(dailyTrendChart, "dailyTrendChart");
    dailyTrendChart->setRenderHint(QPainter::Antialiasing);

    // Group share pie + readable surfaces: caption carries the slice count
    // (Name = its text); the one-column list mirrors the summary Rows.
    groupShareCaption = new QLabel(this);
    groupShareCaption->setObjectName(QStringLiteral("groupShareCaption"));
    auto* shareChart = new QChart;
    shareChart->setTitle(tr("Rows by group"));
    groupShareChart = new QChartView(shareChart, this);
    makeStable(groupShareChart, "groupShareChart");
    groupShareChart->setRenderHint(QPainter::Antialiasing);
    groupShareList = qobject_cast<QListWidget*>(
        makeStable(new QListWidget(this), "groupShareList"));
    groupShareList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    groupShareList->setSelectionMode(QAbstractItemView::NoSelection);

    auto* headerRow = new QHBoxLayout;
    headerRow->addWidget(groupLabel);
    headerRow->addWidget(reportGroupBy);
    headerRow->addStretch(1);
    headerRow->addWidget(exportBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(filterSection);
    layout->addLayout(headerRow);
    layout->addWidget(reportTable, 1);
    auto* shareRow = new QHBoxLayout;
    shareRow->addWidget(groupShareChart, 2);
    shareRow->addWidget(groupShareList, 1);
    layout->addLayout(shareRow, 1);
    layout->addWidget(dailyTrendChart, 1);

    connect(reportGroupBy, &QComboBox::currentTextChanged,
            this, &ReportsView::onGroupChanged);
    connect(reportTable, &QTableWidget::currentCellChanged,
            this, &ReportsView::onSelectionChanged);
    connect(exportBtn, &QPushButton::clicked, this, &ReportsView::onExport);

    refreshSummary();
    if (reportTable->rowCount() > 0)
        reportTable->selectRow(0);
}

ReportsView::CurrentFilters ReportsView::currentFilters() const
{
    CurrentFilters f;
    const auto combo = [](const QComboBox* c) {
        const QString value = c->currentText();
        return (value.isEmpty() || value == QLatin1String("All")) ? QString() : value;
    };
    f.province = combo(provinceCombo);
    f.city = combo(cityCombo);
    f.technology = combo(technologyCombo);
    f.operatorName = combo(operatorCombo);
    f.from = fromDateEdit->date();
    f.to = toDateEdit->date();
    return f;
}

void ReportsView::onProvinceChanged(int index)
{
    Q_UNUSED(index);
    // Cascade (dashboard/network behaviour): city repopulates from the
    // selected province and lands back on All.
    const auto f = currentFilters();
    cityCombo->blockSignals(true);
    cityCombo->clear();
    cityCombo->addItem(QStringLiteral("All"));
    cityCombo->addItems(f.province.isEmpty() ? Db::citiesFor(QString())
                                             : Db::citiesFor(f.province));
    cityCombo->setCurrentIndex(0);
    cityCombo->blockSignals(false);
}

void ReportsView::onSearch()
{
    refreshSummary();
}

void ReportsView::onReset()
{
    provinceCombo->blockSignals(true);
    provinceCombo->setCurrentIndex(0);
    provinceCombo->blockSignals(false);
    onProvinceChanged(0);
    technologyCombo->setCurrentIndex(0);
    operatorCombo->setCurrentIndex(0);
    const auto era = Db::dataEra();
    fromDateEdit->setDate(era.first.isValid() ? era.first : QDate(2000, 1, 1));
    toDateEdit->setDate(era.second.isValid() ? era.second : QDate(2100, 1, 1));
    refreshSummary();
}

void ReportsView::refreshSummary()
{
    const QString column = groupColumnForGroupText(reportGroupBy->currentText());
    const auto f = currentFilters();
    const auto summary =
        Db::avgBy(column, f.province, f.city, f.technology, f.operatorName, f.from, f.to);
    const auto counts =
        Db::countBy(column, f.province, f.city, f.technology, f.operatorName, f.from, f.to);
    // Deterministic order: by label ascending (the table is user-sortable);
    // countBy is ORDER BY label so list and table share one order.
    QStringList labels;
    for (const auto& labelAvg : summary)
        labels << labelAvg.first;
    labels.sort();

    reportTable->setSortingEnabled(false);
    reportTable->setRowCount(0);
    for (const QString& label : labels) {
        const int r = reportTable->rowCount();
        reportTable->insertRow(r);
        reportTable->setItem(r, 0, new QTableWidgetItem(label));
        // Rows per label from the same filtered countBy aggregate.
        int rows = 0;
        for (const auto& count : counts)
            if (count.first == label) {
                rows = count.second;
                break;
            }
        reportTable->setItem(r, 1, new QTableWidgetItem(QString::number(rows)));
        // Avg quality = the shared 0..100 metric over the label's avg dBm.
        double dBm = 0.0;
        for (const auto& labelAvg : summary)
            if (labelAvg.first == label) {
                dBm = labelAvg.second;
                break;
            }
        const double quality = Db::qualityFrom(dBm);
        reportTable->setItem(r, 2, new QTableWidgetItem(QString::number(quality, 'f', 1)));
    }
    reportTable->setSortingEnabled(true);
    reportTable->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);

    // Group share pie: one slice per summary row (countBy), readable via
    // the caption + list, black labels (theme trap, same as the dashboard).
    int total = 0;
    for (const auto& count : counts)
        total += count.second;
    groupShareCaption->setText(
        tr("Rows by %1 - %2 slices")
            .arg(reportGroupBy->currentText(), QString::number(counts.size())));
    groupShareList->clear();
    auto* pie = new QPieSeries;
    for (const auto& count : counts) {
        const double pct = total > 0 ? 100.0 * count.second / total : 0.0;
        groupShareList->addItem(QStringLiteral("%1: %2 rows (%3%)")
                                    .arg(count.first,
                                         QString::number(count.second),
                                         QString::number(pct, 'f', 1)));
        auto* slice = pie->append(count.first, count.second);
        slice->setLabel(QStringLiteral("%1 %2%")
                            .arg(count.first, QString::number(pct, 'f', 1)));
        slice->setLabelVisible(true);
        slice->setLabelBrush(Qt::black);
    }
    auto* shareChartPtr = groupShareChart->chart();
    shareChartPtr->removeAllSeries();
    shareChartPtr->addSeries(pie);

    if (reportTable->rowCount() > 0)
        reportTable->selectRow(0);
    else
        refreshDrill(-1);
}

void ReportsView::onGroupChanged(const QString& group)
{
    Q_UNUSED(group);
    refreshSummary();
}

void ReportsView::onSelectionChanged(int currentRow)
{
    refreshDrill(currentRow);
}

void ReportsView::refreshDrill(int currentRow)
{
    auto* chart = dailyTrendChart->chart();
    chart->removeAllSeries();
    auto* series = new QLineSeries;

    if (currentRow >= 0) {
        const QString column = groupColumnForGroupText(reportGroupBy->currentText());
        const QString label = reportTable->item(currentRow, 0)->text();
        const auto f = currentFilters();
        // The drill reflects the tab's FULL filter state: the selected
        // label drills within the other column filters too.
        const auto trend = Db::dailyQuality(column, label, f.province, f.city,
                                            f.technology, f.operatorName,
                                            f.from, f.to);
        for (const auto& point : trend) {
            const QDateTime day = QDateTime::fromString(point.first, Qt::ISODate);
            series->append(day.toMSecsSinceEpoch(), point.second);
        }
        if (!trend.isEmpty()) {
            timeAxis->setRange(QDateTime::fromString(trend.first().first, Qt::ISODate),
                               QDateTime::fromString(trend.last().first, Qt::ISODate));
            series->setName(label);
        }
    }
    // Points set BEFORE attaching the persistent axis (auto-generated
    // index slots otherwise).
    chart->addSeries(series);
    series->attachAxis(timeAxis);
    series->attachAxis(qualityAxis);
}

void ReportsView::onExport()
{
    const QDir exportDir(QDir(QCoreApplication::applicationDirPath())
                             .absoluteFilePath(QStringLiteral("../../reports/export")));
    if (!exportDir.exists())
        QDir().mkpath(exportDir.absolutePath());
    const QString path = exportDir.absoluteFilePath(
        QStringLiteral("report_")
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))
        + QStringLiteral(".csv"));

    QFile file(path);
    bool ok = file.open(QIODevice::WriteOnly | QIODevice::Text
                        | QIODevice::Truncate);
    if (ok) {
        QTextStream out(&file);
        out << "label,rows,avg_quality\n";
        for (int r = 0; r < reportTable->rowCount(); ++r)
            out << reportTable->item(r, 0)->text() << ','
                << reportTable->item(r, 1)->text() << ','
                << reportTable->item(r, 2)->text() << '\n';
        file.close();
    }
    emit exported(path, ok);
}
