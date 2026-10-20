#include "dashboard_view.h"

#include <QtCharts/QChart>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <QDateTime>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QVBoxLayout>

#include "db.h"

namespace {

QWidget* makeStable(QWidget* widget, const char* id)
{
    widget->setObjectName(QString::fromLatin1(id));
    widget->setAccessibleName(QString::fromLatin1(id));
    return widget;
}

QLabel* kpiValue(QWidget* parent, QVBoxLayout* layout, const char* caption,
                 const char* id)
{
    auto* title = new QLabel(QObject::tr(caption), parent);
    auto* value = qobject_cast<QLabel*>(makeStable(new QLabel(parent), id));
    layout->addWidget(title);
    layout->addWidget(value);
    return value;
}

} // namespace

DashboardView::DashboardView(QWidget* parent)
    : QWidget(parent)
{
    makeStable(this, "dashboardTab");

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

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(kpiRow);
    layout->addWidget(trendChart, 1);
    layout->addWidget(worstCellsList, 1);

    refresh();
}

void DashboardView::refresh()
{
    kpiAvgQuality->setText(QString::number(Db::networkQuality(), 'f', 1)
                           + QStringLiteral("%"));
    kpiCells->setText(QString::number(Db::cellCount()) + QStringLiteral(" cells"));

    // Best/worst operator come from the shared aggregate; ties keep the
    // first label in Db's order.
    const auto byOperator =
        Db::avgBy(QStringLiteral("operator"), QString(), QString(), QString(), QString());
    double bestV = -1e300, worstV = 1e300;
    for (const auto& point : byOperator) {
        if (kpiBestOperator->text() == QString() || point.second > bestV) {
            bestV = point.second;
            kpiBestOperator->setText(point.first);
        }
        if (kpiWorstOperator->text() == QString() || point.second < worstV) {
            worstV = point.second;
            kpiWorstOperator->setText(point.first);
        }
    }

    auto* chart = trendChart->chart();
    chart->removeAllSeries();
    const auto trend = Db::dailyQuality(QString(), QString());
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

    const auto worst = Db::worstCells(8);
    worstCellsList->setRowCount(0);
    for (const auto& row : worst) {
        const int r = worstCellsList->rowCount();
        worstCellsList->insertRow(r);
        for (int c = 0; c < row.size() && c < worstCellsList->columnCount(); ++c)
            worstCellsList->setItem(r, c, new QTableWidgetItem(row.at(c)));
    }
}
