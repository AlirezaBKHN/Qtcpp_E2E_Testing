#include <QApplication>
#include <QPushButton>
#include <QtCharts/QChartView>
#include <QtTest>
#include "mainwindow.h"

// Smoke: MainWindow must construct with every mandatory stable identifier,
// each set on BOTH objectName and accessibleName (plan Global Constraints).
class SmokeMain : public QObject {
    Q_OBJECT
private slots:
    void mandatoryIdentifiersPresent();
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

#include "smoke_main.moc"
