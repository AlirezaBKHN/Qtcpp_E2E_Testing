#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QtCharts/QAbstractBarSeries>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSet>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtTest>
#include "db.h"
#include "mainwindow.h"
#include "signin_dialog.h"
#include "users_view.h"

// Smoke: MainWindow must construct with every mandatory stable identifier,
// each set on BOTH objectName and accessibleName (plan Global Constraints).
class SmokeMain : public QObject {
    Q_OBJECT
private slots:
    void mandatoryIdentifiersPresent();
    void chartShowsValueLabelsAndData();
    void signinGateBlocksMainWindow();
    void registerModeValidation();
    void tabsAndHomeCards();
    void usersViewAdminOnly();
    void dashboardContract();
};

static const char* kIds[] = {
    "provinceCombo", "cityCombo", "technologyCombo", "operatorCombo",
    "searchButton", "refreshButton", "resetButton", "exportButton",
    "signalTable", "signalChart", "chartGroupBy"
};

void SmokeMain::mandatoryIdentifiersPresent()
{
    MainWindow w(QStringLiteral("admin"));
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

// The sign-in dialog gates the app: it is the signinWindow and carries the
// login-mode ids (dialog is constructed directly; the main.cpp loop is the
// runtime gate this slot pins the contract of).
void SmokeMain::signinGateBlocksMainWindow()
{
    SignInDialog d;
    d.show();
    QCOMPARE(d.objectName(), QStringLiteral("signinWindow"));
    QCOMPARE(d.accessibleName(), QStringLiteral("signinWindow"));
    const char* ids[] = {"usernameField", "passwordField", "loginButton",
                         "registerToggle", "signinStatus"};
    for (const char* id : ids) {
        QWidget* found = d.findChild<QWidget*>(QString::fromLatin1(id));
        QVERIFY2(found, qPrintable(QString("missing sign-in child: %1").arg(id)));
        QCOMPARE(QString::fromLatin1(id), found->objectName());
        QCOMPARE(QString::fromLatin1(id), found->accessibleName());
    }
    // Register mode is NOT active initially: registerButton is present for
    // the in-place toggle but hidden; loginButton is the visible action.
    auto* registerButton = d.findChild<QPushButton*>(QString::fromLatin1("registerButton"));
    QVERIFY(registerButton != nullptr);
    QVERIFY2(!registerButton->isVisible(), "register mode active on a fresh dialog");
    auto* loginButton = d.findChild<QPushButton*>(QString::fromLatin1("loginButton"));
    QVERIFY(loginButton != nullptr);
    QVERIFY2(loginButton->isVisible(), "login action must be visible initially");
}

// Register mode validation: only non-empty password + unique username.
void SmokeMain::registerModeValidation()
{
    SignInDialog d;
    d.show();
    auto* toggle = d.findChild<QPushButton*>(QString::fromLatin1("registerToggle"));
    QVERIFY(toggle != nullptr);
    toggle->click();

    auto* registerButton =
        d.findChild<QPushButton*>(QString::fromLatin1("registerButton"));
    QVERIFY2(registerButton != nullptr && registerButton->isVisible(),
             "register mode did not switch in place");
    auto* username =
        d.findChild<QLineEdit*>(QString::fromLatin1("usernameField"));
    QVERIFY(username);
    username->setText(QStringLiteral("smoke_new_user"));

    // Empty password -> pinned copy, no registration attempt.
    auto* registerButtonClicked = registerButton;
    registerButtonClicked->click();
    auto* status = d.findChild<QLabel*>(QString::fromLatin1("registerStatus"));
    QVERIFY(status != nullptr);
    QCOMPARE(status->text(), QStringLiteral("Password can not be empty."));
    QVERIFY(!d.findChild<QLabel*>(QString::fromLatin1("signinStatus"))->isVisible());
}

// Tabbed main window + home routing cards + admin gating.
void SmokeMain::tabsAndHomeCards()
{
    MainWindow admin(QString::fromLatin1("admin"));
    admin.show();
    QTest::qWait(30);

    auto* tabs = admin.findChild<QTabWidget*>(QString::fromLatin1("mainTabs"));
    QVERIFY2(tabs, "mainTabs missing");
    QCOMPARE(QString::fromLatin1("mainTabs"), tabs->objectName());
    QCOMPARE(QString::fromLatin1("mainTabs"), tabs->accessibleName());

    // Pinned tab order: Home, Network Data, Dashboard, Reports, Users(admin).
    const char* pages[] = {"homeTab", "networkTab", "dashboardTab",
                           "reportsTab", "usersTab"};
    for (int i = 0; i < 5; ++i) {
        QVERIFY2(tabs->widget(i) != nullptr, "missing page");
        QCOMPARE(tabs->widget(i)->objectName(), QString::fromLatin1(pages[i]));
        QCOMPARE(tabs->widget(i)->accessibleName(), QString::fromLatin1(pages[i]));
    }
    QCOMPARE(tabs->count(), 5);

    auto* label = admin.findChild<QLabel*>(QString::fromLatin1("currentUserLabel"));
    QVERIFY(label);
    QCOMPARE(label->text(), QStringLiteral("user: admin"));

    auto* logout = admin.findChild<QPushButton*>(QString::fromLatin1("logoutButton"));
    QVERIFY(logout);

    // Cards: admin sees all four.
    for (const char* id : {"homeCardNetwork", "homeCardDashboard",
                           "homeCardReports", "homeCardUsers"}) {
        auto* card = admin.findChild<QPushButton*>(QString::fromLatin1(id));
        QVERIFY2(card, qPrintable(QString("home card missing: %1").arg(id)));
        QCOMPARE(QString::fromLatin1(id), card->objectName());
        QCOMPARE(QString::fromLatin1(id), card->accessibleName());
    }

    // Card switching: homeCardNetwork activates the networkTab page.
    admin.findChild<QPushButton*>(QString::fromLatin1("homeCardNetwork"))->click();
    QCOMPARE(tabs->currentWidget()->objectName(), QStringLiteral("networkTab"));
    QTest::qWait(10);
    // back to home
    tabs->setCurrentIndex(0);
    admin.findChild<QPushButton*>(QString::fromLatin1("homeCardDashboard"))->click();
    QCOMPARE(tabs->currentWidget()->objectName(), QStringLiteral("dashboardTab"));

    // Non-admin: no Users card, no usersTab; tab order pinned (4 tabs).
    MainWindow viewer(QString::fromLatin1("guest"));
    viewer.show();
    QTest::qWait(30);
    QVERIFY(viewer.findChild<QPushButton*>(QString::fromLatin1("homeCardUsers")) == nullptr);
    QVERIFY(viewer.findChild<QTabWidget*>(QString::fromLatin1("mainTabs"))->findChild<QWidget*>(
                QString::fromLatin1("usersTab")) == nullptr);
    QCOMPARE(viewer.findChild<QTabWidget*>(QString::fromLatin1("mainTabs"))->count(), 4);
    QCOMPARE(viewer.findChild<QTabWidget*>(QString::fromLatin1("mainTabs"))
                 ->widget(3)->objectName(), QString::fromLatin1("reportsTab"));
    QCOMPARE(viewer.findChild<QLabel*>(QString::fromLatin1("currentUserLabel"))->text(),
             QStringLiteral("user: guest"));
}

// Users tab: sorted usernames, remove only for admin, admin self-removal guard.
void SmokeMain::usersViewAdminOnly()
{
    // Scratch DB so the committed database/signaltracker.db stays pristine.
    const QString scratch = QStringLiteral("smoke_users_scratch.db");
    QFile::remove(scratch);
    QVERIFY(Db::init(scratch));
    QVERIFY(Db::registerUser(QStringLiteral("smoke_temp"), QStringLiteral("pw123")));
    QStringList expected = Db::usernames();
    QCOMPARE(expected.size(), 4);

    {
        UsersView adminView(QString::fromLatin1("admin"));
        adminView.show();
        auto* table = adminView.findChild<QTableWidget*>(QString::fromLatin1("usersTable"));
        QVERIFY(table);
        QCOMPARE(table->rowCount(), expected.size());
        for (int r = 0; r < expected.size(); ++r)
            QCOMPARE(table->item(r, 0)->text(), expected.at(r));
        auto* btn = adminView.findChild<QPushButton*>(QString::fromLatin1("removeUserButton"));
        QVERIFY(btn);
        QVERIFY(btn->isEnabled());

        // Self-removal guard: 'admin' can never go.
        table->selectRow(expected.indexOf(QStringLiteral("admin")));
        btn->click();
        QVERIFY(Db::usernames().contains(QStringLiteral("admin")));

        // Real removal of a non-admin one-off.
        table->selectRow(Db::usernames().indexOf(QStringLiteral("smoke_temp")));
        btn->click();
        QVERIFY(!Db::usernames().contains(QStringLiteral("smoke_temp")));
        QCOMPARE(table->rowCount(), 3);
        // Every remaining name matches the table.
        for (int r = 0; r < 3; ++r)
            QVERIFY(Db::usernames().contains(table->item(r, 0)->text()));
    }
    {
        UsersView viewerView(QString::fromLatin1("guest"));
        viewerView.show();
        auto* btn = viewerView.findChild<QPushButton*>(QString::fromLatin1("removeUserButton"));
        QVERIFY(btn);
        QVERIFY2(!btn->isEnabled(), "removeUserButton must be disabled for non-admin");
    }
    QFile::remove(scratch);
}

// Dashboard: KPI values equal the Db aggregates, 7-point trend on a
// persistent axis, weakest cells list populated.
void SmokeMain::dashboardContract()
{
    MainWindow admin(QString::fromLatin1("admin"));
    admin.show();
    admin.findChild<QPushButton*>(QString::fromLatin1("homeCardDashboard"))->click();
    QTest::qWait(50);

    auto* chartView = admin.findChild<QChartView*>(QString::fromLatin1("trendChart"));
    QVERIFY2(chartView, "trendChart missing");
    auto* chart = chartView->chart();
    QVERIFY(chart);
    QCOMPARE(chart->series().size(), 1);
    QCOMPARE(chart->axes(Qt::Horizontal).size(), 1);
    QCOMPARE(chart->axes(Qt::Vertical).size(), 1);
    auto* series = qobject_cast<QLineSeries*>(chart->series().first());
    QVERIFY(series != nullptr);
    // Exactly the 7 day buckets the seeder guarantees (Task 2).
    const auto trend = Db::dailyQuality(QString(), QString());
    QCOMPARE(series->count(), trend.size());
    QCOMPARE(series->count(), 7);

    // Persistent year-month-day axis labels.
    auto* axis = qobject_cast<QDateTimeAxis*>(chart->axes(Qt::Horizontal).first());
    QVERIFY(axis != nullptr);
    QCOMPARE(axis->format(), QStringLiteral("yyyy-MM-dd"));

    // KPI values are the Db aggregates, not recomputed UI math.
    auto* kpiAvgQuality = admin.findChild<QLabel*>(QString::fromLatin1("kpiAvgQuality"));
    auto* kpiCells = admin.findChild<QLabel*>(QString::fromLatin1("kpiCells"));
    auto* kpiBest = admin.findChild<QLabel*>(QString::fromLatin1("kpiBestOperator"));
    auto* kpiWorst = admin.findChild<QLabel*>(QString::fromLatin1("kpiWorstOperator"));
    QVERIFY(kpiAvgQuality && kpiCells && kpiBest && kpiWorst);
    QCOMPARE(kpiAvgQuality->text(),
             QString::number(Db::networkQuality(), 'f', 1) + QStringLiteral("%"));
    QCOMPARE(kpiCells->text(),
             QString::number(Db::cellCount()) + QStringLiteral(" cells"));
    const auto byOperator =
        Db::avgBy(QStringLiteral("operator"), QString(), QString(), QString(), QString());
    QString best, worst;
    double bestV = -1e300, worstV = 1e300;
    for (const auto& point : byOperator) {
        if (best.isEmpty() || point.second > bestV) {
            bestV = point.second;
            best = point.first;
        }
        if (worst.isEmpty() || point.second < worstV) {
            worstV = point.second;
            worst = point.first;
        }
    }
    QCOMPARE(kpiBest->text(), best);
    QCOMPARE(kpiWorst->text(), worst);

    auto* worstList =
        admin.findChild<QTableWidget*>(QString::fromLatin1("worstCellsList"));
    QVERIFY(worstList);
    QCOMPARE(worstList->rowCount(), 8);
    QVERIFY(worstList->rowCount() >= 1);
}

// The chart must be readable as data, not just a graphic: per-bar value
// labels visible (outside end, one decimal) and the series must equal the
// aggregates the Db layer produces for the same un-filtered group-by.
void SmokeMain::chartShowsValueLabelsAndData()
{
    MainWindow w(QStringLiteral("admin"));
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
