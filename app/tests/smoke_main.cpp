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
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include "db.h"
#include "mainwindow.h"
#include "signin_dialog.h"
#include "users_view.h"

// Smoke: MainWindow must construct with every mandatory stable identifier,
// each set on BOTH objectName and accessibleName (plan Global Constraints).
class SmokeMain : public QObject {
    Q_OBJECT
private slots:
    void signinDialogEnsuresDomainDb();
    void mandatoryIdentifiersPresent();
    void tableDisplayCappedStatusReal();
    void pagerContract();
    void pagerEmptyResultScratch();
    void chartShowsValueLabelsAndData();
    void signinGateBlocksMainWindow();
    void registerModeValidation();
    void registerSuccessReturnsToLoginScratch();
    void tabsAndHomeCards();
    void usersViewAdminOnly();
    void dashboardContract();
    void reportsContract();
};

static const char* kIds[] = {
    "filterSection", "headerSection",
    "provinceCombo", "cityCombo", "technologyCombo", "operatorCombo",
    "searchButton", "refreshButton", "resetButton", "exportButton",
    "signalTable", "signalChart", "chartGroupBy",
    // Pager (spec 2026-10-20); pageLabel is a READOUT (excluded, like
    // currentUserLabel - its accessibleName carries text, not the id).
    "pagerSection", "pageSizeCombo", "prevPageButton", "nextPageButton"
};

// The readout-contract + gate regression: the sign-in dialog must ensure
// the domain DB on its own (production shows it before any MainWindow, and
// main.cpp inits nothing itself - probed via UIA: without this the default
// connection is absent at first paint, so EVERY login answers "Unknown
// username." and register always fails with "Username already exists.").
// This slot must stay the FIRST slot: it requires a bare default-connection
// state, which later MainWindow-constructing slots would have replaced.
void SmokeMain::signinDialogEnsuresDomainDb()
{
    bool acceptedUserSet = false;
    {
        SignInDialog d;
        d.show();
        QString acceptedUser;
        QObject::connect(&d, &SignInDialog::acceptedWithUser,
                         [&acceptedUser, &acceptedUserSet](const QString& user) {
                             acceptedUser = user; acceptedUserSet = true;
                         });

        // Seeded credentials authenticate at FIRST paint, without any
        // window/main.cpp prior setup: the object accepts and closes.
        d.findChild<QLineEdit*>(QString::fromLatin1("usernameField"))
            ->setText(QStringLiteral("admin"));
        d.findChild<QLineEdit*>(QString::fromLatin1("passwordField"))
            ->setText(QStringLiteral("admin"));
        d.findChild<QPushButton*>(QString::fromLatin1("loginButton"))->click();
        QCOMPARE(acceptedUser, QStringLiteral("admin"));
        QVERIFY(acceptedUserSet);
        QCOMPARE(d.result(), QDialog::Accepted);
    }
    QVERIFY(Db::usernames().size() >= 3); // admin, e2e_user, guest minimum
}

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

// Display cap contract (owner decision 2026-10-20, superseded by the
// pager): the widget now shows page 1 of the paginated query (default page
// size 200), while the status bar keeps reporting the FULL count and the
// CSV export writes the full query. The small UIA tree keeps E2E finds
// fast; the user still sees the real size via the pager.
void SmokeMain::tableDisplayCappedStatusReal()
{
    MainWindow w(QStringLiteral("admin"));
    w.show();
    QTest::qWait(50); // initial fill (ctor onSearch)
    auto* table = w.findChild<QTableWidget*>(QString::fromLatin1("signalTable"));
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 200);

    for (auto* bar : w.findChildren<QStatusBar*>()) {
        const QString text = bar->currentMessage();
        QVERIFY2(text == QStringLiteral("Showing 10080 rows"),
                 qPrintable(QString("status bar says: %1").arg(text)));
    }
}

// Pager contract (spec 2026-10-20): page-size selector 50/100/200/1000
// (default 200), prev/next with edge disabling, page-label math,
// re-query and page-size changes reset to page 1. The old display cap
// is now simply page 1 of the pager.
void SmokeMain::pagerContract()
{
    MainWindow w(QStringLiteral("admin"));
    w.show();
    QTest::qWait(50);
    auto* table = w.findChild<QTableWidget*>(QString::fromLatin1("signalTable"));
    auto* label = w.findChild<QLineEdit*>(QString::fromLatin1("pageLabel"));
    auto* next = w.findChild<QPushButton*>(QString::fromLatin1("nextPageButton"));
    auto* prev = w.findChild<QPushButton*>(QString::fromLatin1("prevPageButton"));
    QVERIFY(table && label && next && prev);
    QCOMPARE(table->rowCount(), 200);
    QCOMPARE(label->text(), QStringLiteral("Page 1 of 51"));
    QCOMPARE(label->accessibleName(), label->text());
    QVERIFY2(!prev->isEnabled(), "prev must be disabled on page 1");

    next->click();
    QCOMPARE(label->text(), QStringLiteral("Page 2 of 51"));
    const auto rows = Db::queryFilters(QString(), QString(), QString(), QString());
    QCOMPARE(table->item(0, 0)->text(), rows.at(200).at(0).toString()); // first id of page 2
    prev->click();
    QCOMPARE(label->text(), QStringLiteral("Page 1 of 51"));

    // Partial last page: size 50 -> 202 pages, last page 30 rows.
    auto* size = w.findChild<QComboBox*>(QString::fromLatin1("pageSizeCombo"));
    QVERIFY(size != nullptr);
    size->setCurrentText(QStringLiteral("50"));
    QCOMPARE(label->text(), QStringLiteral("Page 1 of 202"));
    QCOMPARE(table->rowCount(), 50);
    for (int i = 0; i < 201; ++i)
        next->click();
    QCOMPARE(label->text(), QStringLiteral("Page 202 of 202"));
    QCOMPARE(table->rowCount(), 30);
    QVERIFY2(!next->isEnabled(), "next must be disabled on the last page");

    // Page-size change resets to page 1 (Review Focus 4).
    size->setCurrentText(QStringLiteral("1000"));
    QCOMPARE(label->text(), QStringLiteral("Page 1 of 11"));
    QCOMPARE(table->rowCount(), 1000);
    next->click();
    QCOMPARE(label->text(), QStringLiteral("Page 2 of 11"));

    // A re-query after paging resets to page 1 (Review Focus 3).
    auto* search = w.findChild<QPushButton*>(QString::fromLatin1("searchButton"));
    QVERIFY(search != nullptr);
    search->click();
    QCOMPARE(label->text(), QStringLiteral("Page 1 of 11"));
    QCOMPARE(table->item(0, 0)->text(), rows.at(0).at(0).toString());
}

// Empty result (Review Focus 2): Page 1 of 1, empty table, both pager
// buttons disabled. Scratch DB emptied of cells; default connection
// restored afterwards.
void SmokeMain::pagerEmptyResultScratch()
{
    const QString scratch = QStringLiteral("smoke_pager_scratch.db");
    QFile::remove(scratch);
    QVERIFY(Db::init(scratch));
    QSqlQuery wipe(QSqlDatabase::database());
    QVERIFY(wipe.exec(QStringLiteral("DELETE FROM cells")));

    // MainWindow's ctor re-asks the canonical path; without the override it
    // would replace the scratch connection (same trap as the Task 1
    // register leak) and onSearch would query the working store instead.
    qputenv("IST_DB_PATH", scratch.toUtf8());
    {
        MainWindow w(QStringLiteral("admin"));
        // Unset the override IMMEDIATELY after construction: Db::init has
        // already resolved the path (the connection points at the scratch),
        // and an early-return assertion must not leave the env poisoned for
        // later slots (their ctor re-asks would resolve to the scratch).
        qunsetenv("IST_DB_PATH");
        w.show();
        QTest::qWait(50);
        auto* table = w.findChild<QTableWidget*>(QString::fromLatin1("signalTable"));
        auto* label = w.findChild<QLineEdit*>(QString::fromLatin1("pageLabel"));
        QVERIFY(table && label);
        QCOMPARE(table->rowCount(), 0);
        QCOMPARE(label->text(), QStringLiteral("Page 1 of 1"));
        QVERIFY(!w.findChild<QPushButton*>(QString::fromLatin1("prevPageButton"))->isEnabled());
        QVERIFY(!w.findChild<QPushButton*>(QString::fromLatin1("nextPageButton"))->isEnabled());
    }

    // Restore the working store BEFORE removing the scratch: the open
    // connection holds the file locked on Windows (Task 1 lesson).
    Db::init(Db::defaultDatabasePath());
    QFile::remove(scratch);
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
                         "registerToggle"};
    for (const char* id : ids) {
        QWidget* found = d.findChild<QWidget*>(QString::fromLatin1(id));
        QVERIFY2(found, qPrintable(QString("missing sign-in child: %1").arg(id)));
        QCOMPARE(QString::fromLatin1(id), found->objectName());
        QCOMPARE(QString::fromLatin1(id), found->accessibleName());
    }

    // Status READOUT contract: objectName keeps the stable id, but the
    // accessibleName mirrors the displayed TEXT live. Automation reads the
    // UIA Name (WAD cannot read a QLineEdit's value through any attribute -
    // probed: Value/HelpText/Text all None), so the Name must be the copy.
    auto* signin = d.findChild<QLineEdit*>(QString::fromLatin1("signinStatus"));
    QVERIFY(signin != nullptr);
    QCOMPARE(signin->objectName(), QStringLiteral("signinStatus"));
    QVERIFY(signin->isReadOnly());
    QCOMPARE(signin->accessibleName(), signin->text()); // fresh: ""
    auto* username = d.findChild<QLineEdit*>(QString::fromLatin1("usernameField"));
    QVERIFY(username != nullptr);
    auto* loginButton = d.findChild<QPushButton*>(QString::fromLatin1("loginButton"));
    QVERIFY(loginButton != nullptr);
    username->setText(QStringLiteral("ghost_user"));
    loginButton->click();
    QCOMPARE(signin->text(), QStringLiteral("Unknown username."));
    QCOMPARE(signin->accessibleName(), QStringLiteral("Unknown username."));
    // Register mode is NOT active initially: registerButton is present for
    // the in-place toggle but hidden; loginButton is the visible action.
    auto* registerButton = d.findChild<QPushButton*>(QString::fromLatin1("registerButton"));
    QVERIFY(registerButton != nullptr);
    QVERIFY2(!registerButton->isVisible(), "register mode active on a fresh dialog");
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
    auto* status = d.findChild<QLineEdit*>(QString::fromLatin1("registerStatus"));
    QVERIFY(status != nullptr);
    QCOMPARE(status->text(), QStringLiteral("Password can not be empty."));
    QCOMPARE(status->accessibleName(), status->text());
    QVERIFY(!d.findChild<QLineEdit*>(QString::fromLatin1("signinStatus"))->isVisible());
}

// Register success (plan Task 3 pin): the dialog returns to the LOGIN
// screen carrying the pinned copy on the login readout, with the fields
// cleared (probed state machine; E2E TC011 pins the same loop through the
// DSL). Scratch DB so the committed store stays pristine.
void SmokeMain::registerSuccessReturnsToLoginScratch()
{
    const QString scratch = QStringLiteral("smoke_users_scratch.db");
    QFile::remove(scratch);

    // SignInDialog's ctor ensures the DEFAULT store (committed DB). It must
    // run BEFORE the connection is pointed at the scratch - otherwise it
    // silently re-points the default connection away from the scratch and
    // the register INSERT below lands in the committed store (observed:
    // smoke_reg_success leaked into database/signaltracker.db, and every
    // later run failed with "Username already exists.").
    SignInDialog d;
    d.show();
    QVERIFY(Db::init(scratch));
    auto* toggle = d.findChild<QPushButton*>(QString::fromLatin1("registerToggle"));
    QVERIFY(toggle != nullptr);
    toggle->click();

    auto* username = d.findChild<QLineEdit*>(QString::fromLatin1("usernameField"));
    auto* password = d.findChild<QLineEdit*>(QString::fromLatin1("passwordField"));
    QVERIFY(username && password);
    username->setText(QStringLiteral("smoke_reg_success"));
    password->setText(QStringLiteral("pw123456"));

    auto* registerButton =
        d.findChild<QPushButton*>(QString::fromLatin1("registerButton"));
    QVERIFY(registerButton != nullptr);
    registerButton->click();

    // Back on the login screen: register controls hidden, the login
    // readout carries the pinned copy, fields cleared.
    QVERIFY2(!registerButton->isVisible(),
             "register success did not return to the login screen");
    auto* signin = d.findChild<QLineEdit*>(QString::fromLatin1("signinStatus"));
    QVERIFY(signin != nullptr);
    QCOMPARE(signin->text(), QStringLiteral("Registered \u2014 log in."));
    QCOMPARE(signin->accessibleName(), signin->text());
    QCOMPARE(username->text(), QString());
    QCOMPARE(password->text(), QString());

    // Re-point at the default store BEFORE removing the scratch: the open
    // connection holds the file locked on Windows, so the removal would
    // fail silently and a leftover scratch would leak into later slots
    // (Db::init's same-path guard would then treat it as already set up).
    Db::init(Db::defaultDatabasePath());
    QFile::remove(scratch);
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

    auto* label = admin.findChild<QLineEdit*>(QString::fromLatin1("currentUserLabel"));
    QVERIFY(label);
    QCOMPARE(label->text(), QStringLiteral("user: admin"));
    // Top-bar readout: accessibleName mirrors the text (automation contract).
    QCOMPARE(label->accessibleName(), label->text());

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
    QCOMPARE(viewer.findChild<QLineEdit*>(QString::fromLatin1("currentUserLabel"))->text(),
             QStringLiteral("user: guest"));
    QCOMPARE(viewer.findChild<QLineEdit*>(QString::fromLatin1("currentUserLabel"))
                 ->accessibleName(), QStringLiteral("user: guest"));
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
    auto* kpiAvgQuality = admin.findChild<QLineEdit*>(QString::fromLatin1("kpiAvgQuality"));
    auto* kpiCells = admin.findChild<QLineEdit*>(QString::fromLatin1("kpiCells"));
    auto* kpiBest = admin.findChild<QLineEdit*>(QString::fromLatin1("kpiBestOperator"));
    auto* kpiWorst = admin.findChild<QLineEdit*>(QString::fromLatin1("kpiWorstOperator"));
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
    // KPI readouts mirror their text in the accessibleName (automation).
    QCOMPARE(kpiAvgQuality->accessibleName(), kpiAvgQuality->text());
    QCOMPARE(kpiCells->accessibleName(), kpiCells->text());
    QCOMPARE(kpiBest->accessibleName(), kpiBest->text());
    QCOMPARE(kpiWorst->accessibleName(), kpiWorst->text());

    auto* worstList =
        admin.findChild<QTableWidget*>(QString::fromLatin1("worstCellsList"));
    QVERIFY(worstList);
    QCOMPARE(worstList->rowCount(), 8);
    QVERIFY(worstList->rowCount() >= 1);
}

// Reports: group summary table + per-label drill-down + CSV export.
void SmokeMain::reportsContract()
{
    MainWindow admin(QString::fromLatin1("admin"));
    admin.show();
    admin.findChild<QPushButton*>(QString::fromLatin1("homeCardReports"))->click();
    QTest::qWait(30);

    auto* groupBy = admin.findChild<QComboBox*>(QString::fromLatin1("reportGroupBy"));
    QVERIFY(groupBy);
    QCOMPARE(groupBy->currentText(), QStringLiteral("Province"));

    auto* table = admin.findChild<QTableWidget*>(QString::fromLatin1("reportTable"));
    QVERIFY(table);
    QCOMPARE(table->columnCount(), 3);
    // One row per province, labels label-ascending (the sortable table's
    // ruling), positive row counts.
    QStringList provinces = Db::provinces();
    provinces.sort();
    QCOMPARE(table->rowCount(), Db::provinces().size());
    const int rowCount = table->rowCount();
    for (int r = 0; r < rowCount; ++r) {
        QCOMPARE(table->item(r, 0)->text(), provinces.at(r));
        QVERIFY(table->item(r, 1)->text().toInt() > 0);
        QVERIFY(!table->item(r, 2)->text().isEmpty());
    }

    // Drill-down: selecting a row makes the trend chart show that label's
    // per-day quality - exactly the 7 seeded buckets.
    auto* chartView = admin.findChild<QChartView*>(QString::fromLatin1("dailyTrendChart"));
    QVERIFY(chartView);
    auto* chart = chartView->chart();
    QCOMPARE(chart->series().size(), 1);
    QCOMPARE(chart->axes(Qt::Horizontal).size(), 1);
    QCOMPARE(chart->axes(Qt::Vertical).size(), 1);
    table->selectRow(0);
    QTest::qWait(30);
    auto* series = qobject_cast<QLineSeries*>(chart->series().first());
    QVERIFY(series != nullptr);
    QCOMPARE(series->count(), 7);
    QTest::qWait(10);

    // Export: silent writer into reports/export/report_*.csv with a header.
    const QDir exportDir(QDir(QCoreApplication::applicationDirPath())
                             .absoluteFilePath(QStringLiteral("../../reports/export")));
    if (exportDir.exists())
        for (const QFileInfo& f : exportDir.entryInfoList({"report_*.csv"}))
            QFile::remove(f.absoluteFilePath());
    auto* exportButton =
        admin.findChild<QPushButton*>(QString::fromLatin1("exportReportButton"));
    QVERIFY(exportButton);
    exportButton->click();
    QTest::qWait(50);
    const QStringList written =
        exportDir.entryList({"report_*.csv"}, QDir::Files);
    QCOMPARE(written.size(), 1);
    QFile out(exportDir.absoluteFilePath(written.first()));
    QVERIFY(out.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream in(&out);
    QCOMPARE(in.readLine(), QStringLiteral("label,rows,avg_quality"));
    out.close();
    // Status bar confirms with the conventional copy.
    bool confirmed = false;
    for (auto* bar : admin.findChildren<QStatusBar*>()) {
        if (bar->currentMessage().startsWith(QStringLiteral("Exported: ")))
            confirmed = true;
    }
    QVERIFY(confirmed);
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
