#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QSplitter>
#include <QtCharts/QAbstractBarSeries>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSet>
#include <QtCharts/QChartView>
#include <QtCharts/QValueAxis>
#include <QtTest>
#include "db.h"
#include "mainwindow.h"

// Smoke: MainWindow must construct with every mandatory stable identifier,
// each set on BOTH objectName and accessibleName (plan Global Constraints).
class SmokeMain : public QObject {
    Q_OBJECT
private slots:
    void mandatoryIdentifiersPresent();
    void chartShowsValueLabelsAndData();
};

static const char* kIds[] = {
    "provinceCombo", "cityCombo", "technologyCombo", "operatorCombo",
    "searchButton", "refreshButton", "resetButton", "exportButton",
    "signalTable", "signalChart", "chartGroupBy"
};

void SmokeMain::mandatoryIdentifiersPresent()
{
    MainWindow w;
    w.show();
    for (const char* id : kIds) {
        QWidget* found = w.findChild<QWidget*>(QString::fromLatin1(id));
        QVERIFY2(found != nullptr, qPrintable(QString("missing child: %1").arg(id)));
        QCOMPARE(QString::fromLatin1(id), found->objectName());
        QCOMPARE(QString::fromLatin1(id), found->accessibleName());
    }

    // After the initial fill, the status bar must report the row count so UI
    // automation can read a stable, fast count (UIA realizes only visible rows).
    QTest::qWait(50);
    for (auto* bar : w.findChildren<QStatusBar*>()) {
        const QString text = bar->currentMessage();
        QVERIFY2(text.startsWith(QStringLiteral("Showing ")),
                 qPrintable(QString("status bar says: %1").arg(text)));
        QVERIFY2(text.contains(QStringLiteral(" rows")),
                 qPrintable(QString("status bar says: %1").arg(text)));
    }

    // The chart must stay healthy across repeated searches: exactly one
    // series, exactly one axis per edge, readable label font.
    auto* chartView = w.findChild<QChartView*>(QString::fromLatin1("signalChart"));
    QVERIFY(chartView != nullptr);
    auto* chart = chartView->chart();
    QVERIFY(chart != nullptr);

    // Second full search refresh must not pile up axes.
    for (auto* bar : w.findChildren<QStatusBar*>()) {
        const QString text = bar->currentMessage();
        if (text.startsWith(QStringLiteral("Showing ")))
            QVERIFY2(w.findChild<QPushButton*>(QString::fromLatin1("searchButton")) != nullptr,
                     "search button missing");
    }
    QTest::qWait(20);
    {
        QPushButton* search = w.findChild<QPushButton*>(QString::fromLatin1("searchButton"));
        QVERIFY(search != nullptr);
        search->click(); // refresh #2 (also exercises the refres-chart path)
        QTest::qWait(20);
        QCOMPARE(chart->series().size(), 1);
        QCOMPARE(chart->axes(Qt::Horizontal).size(), 1);
        QCOMPARE(chart->axes(Qt::Vertical).size(), 1);
        const auto axesY = chart->axes(Qt::Vertical);
        QCOMPARE(axesY.first()->labelsFont().pointSize(), 9);
    }
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    SmokeMain t;
    return QTest::qExec(&t, argc, argv);
}

// The chart must be readable as data, not just a graphic: per-bar value
// labels visible (outside end, one decimal) and the series must equal the
// aggregates the Db layer produces for the same un-filtered group-by.
void SmokeMain::chartShowsValueLabelsAndData()
{
    MainWindow w;
    w.show();
    QTest::qWait(50);

    auto* chartView = w.findChild<QChartView*>(QString::fromLatin1("signalChart"));
    QVERIFY(chartView != nullptr);
    auto* chart = chartView->chart();
    QCOMPARE(chart->series().size(), 1);
    auto* series = qobject_cast<QAbstractBarSeries*>(chart->series().first());
    QVERIFY(series != nullptr);

    // Value labels readable on each bar (owner requirement). Qt renders
    // bar labels as '@value'-tagged text with 'g' significant digits;
    // "%.1f" has no tag and is dropped by the rich-text label item.
    QVERIFY2(series->isLabelsVisible(), "bar labels must be visible");
    QCOMPARE(series->labelsPosition(), QAbstractBarSeries::LabelsOutsideEnd);
    QCOMPARE(series->labelsFormat(), QStringLiteral("@value"));
    QCOMPARE(series->labelsPrecision(), 3);

    // Series data must equal the quality metric the Db aggregates encode:
    // bars are 0..100 quality, quality = (dBm + 120) / 0.7 (i.e. /70*100),
    // because the bar canvas draws bars from the implied ZERO line and an
    // all-negative dBm domain clips bars+labels at the top edge.
    const QVector<LabelAvg> points =
        Db::avgBy(QStringLiteral("operator"), QString(), QString(), QString(), QString());
    QVERIFY(!points.isEmpty());
    QCOMPARE(series->count(), 1);
    QBarSet* barSet = series->barSets().first();
    QCOMPARE(barSet->count(), points.size());
    for (int i = 0; i < points.size(); ++i) {
        const double expectedQuality = (points.at(i).second + 120.0) / 0.7;
        QVERIFY2(qAbs(barSet->at(i) - expectedQuality) < 1e-9,
                 qPrintable(QString("quality %1 vs expected %2")
                                .arg(barSet->at(i)).arg(expectedQuality)));
    }

    // Y axis: flat 0..100 with neat 25% ticks.
    auto* axisY = qobject_cast<QValueAxis*>(chart->axes(Qt::Vertical).first());
    QVERIFY(axisY != nullptr);
    QCOMPARE(axisY->min(), 0.0);
    QCOMPARE(axisY->max(), 100.0);
    QCOMPARE(axisY->tickCount(), 5);

    // X categories must carry the group labels, not elided dots.
    auto* axisX = qobject_cast<QBarCategoryAxis*>(chart->axes(Qt::Horizontal).first());
    QVERIFY(axisX != nullptr);
    QStringList expectedCategories;
    for (const auto& point : points)
        expectedCategories << point.first;
    QCOMPARE(axisX->categories(), expectedCategories);
}


#include "smoke_main.moc"
