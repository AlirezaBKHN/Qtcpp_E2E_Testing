#include <QApplication>
#include <QTest>
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
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    SmokeMain t;
    return QTest::qExec(&t, argc, argv);
}

#include "smoke_main.moc"
