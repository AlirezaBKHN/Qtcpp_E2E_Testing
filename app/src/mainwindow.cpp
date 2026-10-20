#include "dashboard_view.h"
#include "mainwindow.h"
#include "reports_view.h"
#include "users_view.h"

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QValueAxis>

#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "db.h"

namespace {

constexpr char kTitle[] = "Iran Signal Tracker";

// Sets the stable identifier on BOTH objectName and accessibleName.
QWidget* makeStable(QWidget* widget, const char* id)
{
    widget->setObjectName(QString::fromLatin1(id));
    widget->setAccessibleName(QString::fromLatin1(id));
    return widget;
}

QComboBox* makeCombo(QWidget* parent, const char* id, const QStringList& values,
                     bool withAll = true)
{
    auto* combo = new QComboBox(parent);
    makeStable(combo, id);
    if (withAll)
        combo->addItem(QStringLiteral("All"));
    combo->addItems(values);
    return combo;
}

// Clears and reloads the combo while preserving the "All" behaviour of
// cascade transitions: after repopulating the selection always lands on All.
void repopulateCombo(QComboBox* combo, const QStringList& values)
{
    combo->blockSignals(true);
    combo->clear();
    combo->addItem(QStringLiteral("All"));
    combo->addItems(values);
    combo->setCurrentIndex(0);
    combo->blockSignals(false);
}

} // namespace

MainWindow::MainWindow(const QString& user, QWidget* parent)
    : QMainWindow(parent)
    , currentUser_(user)
{
    setWindowTitle(QString::fromLatin1(kTitle));
    resize(1100, 700);

    // Populate domain data from the single source of truth.
    const QString dbPath = QDir(QCoreApplication::applicationDirPath())
                               .absoluteFilePath(QStringLiteral("../../database/signaltracker.db"));
    Db::init(dbPath);

    // -- Filter section -----------------------------------------------------
    provinceCombo = makeCombo(this, "provinceCombo", Db::provinces());
    cityCombo = makeCombo(this, "cityCombo", Db::citiesFor(QString()));
    technologyCombo = makeCombo(this, "technologyCombo", {"2G", "3G", "4G", "5G"});
    operatorCombo = makeCombo(this, "operatorCombo",
                              {"MCI", "Irancell", "Rightel", "Shatel Mobile"});

    auto* searchButtonPtr = new QPushButton(tr("Search"), this);
    searchButton = qobject_cast<QPushButton*>(makeStable(searchButtonPtr, "searchButton"));
    searchButton->setDefault(true);
    auto* refreshButtonPtr = new QPushButton(tr("Refresh"), this);
    refreshButton = qobject_cast<QPushButton*>(makeStable(refreshButtonPtr, "refreshButton"));
    auto* resetButtonPtr = new QPushButton(tr("Reset"), this);
    resetButton = qobject_cast<QPushButton*>(makeStable(resetButtonPtr, "resetButton"));
    auto* exportButtonPtr = new QPushButton(tr("Export CSV"), this);
    exportButton = qobject_cast<QPushButton*>(makeStable(exportButtonPtr, "exportButton"));

    auto* filterRow = new QHBoxLayout;
    filterRow->addWidget(provinceCombo);
    filterRow->addWidget(cityCombo);
    filterRow->addWidget(technologyCombo);
    filterRow->addWidget(operatorCombo);
    filterRow->addWidget(searchButton);
    filterRow->addWidget(refreshButton);
    filterRow->addWidget(resetButton);
    filterRow->addWidget(exportButton);
    filterRow->addStretch(1);

    // -- Table --------------------------------------------------------------
    signalTable = qobject_cast<QTableWidget*>(
        makeStable(new QTableWidget(0, 7, this), "signalTable"));
    const QStringList headers = {tr("ID"), tr("Province"), tr("City"),
                                 tr("Operator"), tr("Technology"),
                                 tr("Signal Strength"), tr("Timestamp")};
    signalTable->setHorizontalHeaderLabels(headers);
    signalTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    signalTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    signalTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // -- Chart --------------------------------------------------------------
    chartGroupBy = makeCombo(this, "chartGroupBy", {tr("Operator"), tr("Technology")},
                             false);
    chartGroupBy->setCurrentIndex(0);

    auto* chart = new QChart;
    chart->setTitle(tr("Average Signal Strength"));
    // Legend on the right: at the default pane height a bottom legend ate
    // the plot band and every axis label was elided to dots.
    chart->legend()->setAlignment(Qt::AlignRight);
    chart->legend()->show();
    signalChart = new QChartView(chart, this);
    makeStable(signalChart, "signalChart");
    signalChart->setRenderHint(QPainter::Antialiasing);

    // Persistent axes: created exactly once; refreshChart only replaces the
    // series and refills categories. Creating axes per-refresh piled them up
    // (overlapping dot-elided labels, broken resize repaint).
    axisX = new QBarCategoryAxis;
    axisY = new QValueAxis;
    // Quality scale 0..100; 5 ticks = flat 25% steps.
    axisY->setRange(0, 100);
    axisY->setTitleText(QStringLiteral("Quality (%)"));
    axisY->setLabelFormat(QStringLiteral("%d"));
    QFont axisFont(QStringLiteral("Segoe UI"), 9);
    axisX->setLabelsFont(axisFont);
    axisY->setLabelsFont(axisFont);
    axisY->setTickCount(5);
    chart->addAxis(axisX, Qt::AlignBottom);
    chart->addAxis(axisY, Qt::AlignLeft);

    auto* chartRow = new QHBoxLayout;
    chartRow->addWidget(chartGroupBy);
    chartRow->addWidget(signalChart, 1);

    // -- Assembly: tabbed main window ---------------------------------------
    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(signalTable);

    auto* chartPane = new QWidget(this);
    chartPane->setLayout(chartRow);
    splitter->addWidget(chartPane);
    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 3);
    // Initial pane sizes: with stretch alone the chart pane came up as a
    // ~180px sliver, leaving a ~30px plot band where every axis label was
    // elided to "…" (screenshot evidence). 380/260 keeps the plot readable.
    splitter->setSizes({380, 260});

    // Top bar: current-user readout + logout.
    auto* currentUserLabel =
        qobject_cast<QLabel*>(makeStable(new QLabel(this), "currentUserLabel"));
    currentUserLabel->setText(QStringLiteral("user: ") + currentUser_);
    auto* logoutButton =
        qobject_cast<QPushButton*>(makeStable(new QPushButton(tr("Log out"), this),
                                              "logoutButton"));
    connect(logoutButton, &QPushButton::clicked, this, &MainWindow::onLogout);

    auto* topBar = new QHBoxLayout;
    topBar->addWidget(currentUserLabel);
    topBar->addStretch(1);
    topBar->addWidget(logoutButton);

    // -- Network Data page: the original central layout, verbatim.
    QWidget* networkPage = makeStable(new QWidget(this), "networkTab");
    auto* networkLayout = new QVBoxLayout(networkPage);
    networkLayout->addLayout(filterRow);
    networkLayout->addWidget(splitter, 1);

    // Dashboard/Reports arrive in Tasks 5/6; placeholders pin the tab order
    // now (replaced in place, never reordered).
    // Home page + real dashboard and reports views.
    QWidget* homePage = makeStable(new QWidget(this), "homeTab");
    QWidget* dashboardPage = new DashboardView(this);
    auto* reportsView = new ReportsView(this);
    QWidget* reportsPage = reportsView;

    // -- Home: routing cards.
    auto* cardsRow = new QHBoxLayout;
    const std::initializer_list<QPair<const char*, QPair<int, const char*>>> cards = {
        {"homeCardNetwork", {1, "Network Data"}},
        {"homeCardDashboard", {2, "Dashboard"}},
        {"homeCardReports", {3, "Reports"}},
    };
    for (const auto& [id, target] : cards) {
        auto* card = qobject_cast<QPushButton*>(makeStable(
            new QPushButton(QString::fromLatin1(target.second), homePage), id));
        const int index = target.first;
        connect(card, &QPushButton::clicked, this, [this, index] {
            mainTabs->setCurrentIndex(index);
        });
        cardsRow->addWidget(card);
    }
    if (currentUser_ == QLatin1String("admin")) {
        auto* card = qobject_cast<QPushButton*>(makeStable(
            new QPushButton(tr("Users"), homePage), "homeCardUsers"));
        const int index = 4;
        connect(card, &QPushButton::clicked, this, [this, index] {
            mainTabs->setCurrentIndex(index);
        });
        cardsRow->addWidget(card);
    }
    cardsRow->addStretch(1);
    auto* homeLayout = new QVBoxLayout(homePage);
    homeLayout->addLayout(cardsRow);
    homeLayout->addStretch(1);

    mainTabs = qobject_cast<QTabWidget*>(makeStable(new QTabWidget(this), "mainTabs"));
    mainTabs->addTab(homePage, tr("Home"));
    mainTabs->addTab(networkPage, tr("Network Data"));
    mainTabs->addTab(dashboardPage, tr("Dashboard"));
    mainTabs->addTab(reportsPage, tr("Reports"));
    if (currentUser_ == QLatin1String("admin"))
        mainTabs->addTab(new UsersView(currentUser_, this), tr("Users"));
    // The usersTab page id is pinned on the mounted view (admin only).
    if (currentUser_ == QLatin1String("admin")) {
        QWidget* usersPage = mainTabs->widget(4);
        if (usersPage && usersPage->objectName().isEmpty())
            makeStable(usersPage, "usersTab");
    }

    auto* central = new QWidget(this);
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->addLayout(topBar);
    centralLayout->addWidget(mainTabs, 1);
    setCentralWidget(central);

    statusBar()->showMessage(tr("Ready"));

    connect(searchButton, &QPushButton::clicked, this, &MainWindow::onSearch);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::onRefresh);
    connect(resetButton, &QPushButton::clicked, this, &MainWindow::onReset);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::onExport);
    connect(reportsView, &ReportsView::exported, this, [this](const QString& path, bool ok) {
        statusBar()->showMessage(ok ? QStringLiteral("Exported: ") + path
                                    : QStringLiteral("Export failed"));
    });
    connect(provinceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onProvinceChanged);
    connect(chartGroupBy, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this] { refreshChart(); });

    // Initial view shows the full dataset.
    onSearch();
}

void MainWindow::onSearch()
{
    auto current = [](QComboBox* combo) {
        return combo->currentIndex() == 0 && combo->findText(QStringLiteral("All")) == 0
                   ? QString()
                   : combo->currentText();
    };
    // chartGroupBy has no "All" entry, so it always reports its text.
    const auto rows = Db::queryFilters(current(provinceCombo), current(cityCombo),
                                       current(technologyCombo), current(operatorCombo));
    signalTable->setRowCount(0);
    for (const auto& row : rows) {
        const int r = signalTable->rowCount();
        signalTable->insertRow(r);
        for (int c = 0; c < row.size() && c < signalTable->columnCount(); ++c)
            signalTable->setItem(r, c, new QTableWidgetItem(row.at(c).toString()));
    }
    statusBar()->showMessage(
        QStringLiteral("Showing %1 rows").arg(rows.size()));
    refreshChart();
}

void MainWindow::onRefresh()
{
    onSearch();
}

void MainWindow::onReset()
{
    repopulateCombo(cityCombo, Db::citiesFor(QString()));
    provinceCombo->setCurrentIndex(0);
    cityCombo->setCurrentIndex(0);
    technologyCombo->setCurrentIndex(0);
    operatorCombo->setCurrentIndex(0);
    onSearch();
}

void MainWindow::onExport()
{
    qWarning() << "onExport invoked";
    const QDir exportDir(QDir(QCoreApplication::applicationDirPath())
                             .absoluteFilePath(QStringLiteral("../../reports/export")));
    if (!exportDir.exists())
        QDir().mkpath(exportDir.absolutePath());
    const QString path = exportDir.absoluteFilePath(
        QStringLiteral("export_") + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")
        + QStringLiteral(".csv"));
    const bool ok = Db::writeExportCsv(
        path,
        provinceCombo->currentIndex() == 0 ? QString() : provinceCombo->currentText(),
        cityCombo->currentIndex() == 0 ? QString() : cityCombo->currentText(),
        technologyCombo->currentIndex() == 0 ? QString() : technologyCombo->currentText(),
        operatorCombo->currentIndex() == 0 ? QString() : operatorCombo->currentText());
    statusBar()->showMessage(ok ? QStringLiteral("Exported: ") + path
                                : QStringLiteral("Export failed"));
}

void MainWindow::onProvinceChanged(int index)
{
    // Cascade: city list narrows to the selected province ("All" keeps all).
    if (index <= 0)
        repopulateCombo(cityCombo, Db::citiesFor(QString()));
    else
        repopulateCombo(cityCombo, Db::citiesFor(provinceCombo->itemText(index)));
}

void MainWindow::refreshChart()
{
    auto* chart = signalChart->chart();
    chart->removeAllSeries();

    const auto group = chartGroupBy->currentIndex() == 0
                           ? QStringLiteral("operator")
                           : QStringLiteral("technology");
    const auto current = [](QComboBox* combo) {
        return combo->currentIndex() == 0 ? QString() : combo->currentText();
    };
    const auto points = Db::avgBy(group, current(provinceCombo), current(cityCombo),
                                  current(technologyCombo), current(operatorCombo));

    auto* series = new QBarSeries;
    // Value labels on each bar (owner requirement). Qt bar labels are
    // "@value"-tagged text rendered with 'g'-style significant digits;
    // a printf format like "%.1f" has no tag, renders literally and the
    // rich-text item drops it (observed: no labels at all).
    series->setLabelsVisible(true);
    series->setLabelsPosition(QAbstractBarSeries::LabelsOutsideEnd);
    series->setLabelsFormat(QStringLiteral("@value"));
    series->setLabelsPrecision(3);
    auto* barSet = new QBarSet(tr("Quality (%)"));
    // Theme picks the label brush from the *opposite end* of the bar
    // gradient - near-white for a blue bar - over a white plot background
    // the labels were present but invisible. Explicit black matches every
    // other chart label.
    barSet->setLabelBrush(Qt::black);
    QStringList categories;
    // Bars encode a 0..100 quality metric, not raw dBm: a bar-series item
    // draws bars from the implied ZERO line, and for an all-negative dBm
    // domain (-120..-40) that line sits above the plot — bars hung from the
    // top edge and every label collapsed onto one clipped line (screenshot
    // + scene-dump evidence). quality = (dBm + 120) / 70 * 100.
    for (const auto& point : points) {
        categories << point.first;
        *barSet << ((point.second + 120.0) / 0.7);
    }
    // Categories MUST be set before the series attaches: an attached
    // category axis synced to a series with no categories auto-generates
    // index placeholders ("1","2",…), doubling the slots and misaligning
    // every label (screenshot: dots across the whole axis).
    axisX->clear();
    axisX->append(categories);
    series->append(barSet);
    chart->addSeries(series);
    series->attachAxis(axisX);
    series->attachAxis(axisY);

    chart->setTitle(group == QStringLiteral("operator")
                        ? tr("Average Signal Quality by Operator")
                        : tr("Average Signal Quality by Technology"));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // A close that did not come from a logout means the user quit the app:
    // the sign-in loop in main.cpp exits. Logout first sets loggingOut_ so
    // the loop instead reopens SignInDialog.
    if (!loggingOut_)
        shouldExit_ = true;
    QMainWindow::closeEvent(event);
}

void MainWindow::onLogout()
{
    loggingOut_ = true;
    close();
}
