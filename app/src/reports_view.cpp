#include "reports_view.h"

#include <QtCharts/QChart>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
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

    auto* headerRow = new QHBoxLayout;
    headerRow->addWidget(groupLabel);
    headerRow->addWidget(reportGroupBy);
    headerRow->addStretch(1);
    headerRow->addWidget(exportBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(headerRow);
    layout->addWidget(reportTable, 1);
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

void ReportsView::refreshSummary()
{
    const QString column = groupColumnForGroupText(reportGroupBy->currentText());
    const auto summary =
        Db::avgBy(column, QString(), QString(), QString(), QString());
    // Deterministic order: by label ascending (the table is user-sortable).
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
        // Rows per label: run the single-column filter for the group type.
        const int rows = Db::queryFilters(
                             column == QLatin1String("province") ? label : QString(),
                             QString(),
                             column == QLatin1String("technology") ? label : QString(),
                             column == QLatin1String("operator") ? label : QString())
                             .size();
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
        const auto trend = Db::dailyQuality(column, label);
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
