#include "mainwindow.h"

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QValueAxis>

#include <QDateTime>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QSplitter>
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

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
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
    chart->legend()->show();
    signalChart = new QChartView(chart, this);
    makeStable(signalChart, "signalChart");
    signalChart->setRenderHint(QPainter::Antialiasing);

    auto* chartRow = new QHBoxLayout;
    chartRow->addWidget(chartGroupBy);
    chartRow->addWidget(signalChart, 1);

    // -- Assembly -----------------------------------------------------------
    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(signalTable);

    auto* chartPane = new QWidget(this);
    chartPane->setLayout(chartRow);
    splitter->addWidget(chartPane);
    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 3);

    auto* central = new QWidget(this);
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->addLayout(filterRow);
    centralLayout->addWidget(splitter, 1);
    setCentralWidget(central);

    statusBar()->showMessage(tr("Ready"));

    connect(searchButton, &QPushButton::clicked, this, &MainWindow::onSearch);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::onRefresh);
    connect(resetButton, &QPushButton::clicked, this, &MainWindow::onReset);
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::onExport);
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
    auto* barSet = new QBarSet(tr("Avg signal (dBm)"));
    QStringList categories;
    for (const auto& point : points) {
        categories << point.first;
        *barSet << point.second;
    }
    series->append(barSet);
    chart->addSeries(series);

    auto* axisX = new QBarCategoryAxis;
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* axisY = new QValueAxis;
    axisY->setRange(-120, -50);
    axisY->setTitleText(QStringLiteral("dBm"));
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart->setTitle(group == QStringLiteral("operator")
                        ? tr("Average Signal Strength by Operator")
                        : tr("Average Signal Strength by Technology"));
}
