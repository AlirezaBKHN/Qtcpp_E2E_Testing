#include "mainwindow.h"

#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QBarSeries>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QValueAxis>

#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

#include "db.h"

namespace {

constexpr char kTitle[] = "Iran Signal Tracker";

const char* kProvinces[] = {"Tehran",      "Isfahan", "Fars",    "Mazandaran",
                            "Khorasan Razavi", "East Azerbaijan", "West Azerbaijan",
                            "Khuzestan",  "Gilan",   "Kerman"};

const char* kCities[] = {"Tehran", "Karaj", "Qom",   "Isfahan", "Kashan",
                         "Shiraz", "Sari",  "Amol",  "Mashhad", "Neyshabur",
                         "Tabriz", "Urmia", "Ahvaz", "Abadan",  "Rasht",
                         "Kerman", "Bam"};

const char* kTechnologies[] = {"2G", "3G", "4G", "5G"};

const char* kOperators[] = {"MCI", "Irancell", "Rightel", "Shatel Mobile"};

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
    for (const auto& value : values)
        combo->addItem(value);
    return combo;
}

void fillCombo(QComboBox* combo, const QStringList& values)
{
    combo->blockSignals(true);
    const QString current = combo->currentText();
    combo->clear();
    combo->addItem(QStringLiteral("All"));
    combo->addItems(values);
    const int index = combo->findText(current);
    combo->setCurrentIndex(index < 0 ? 0 : index);
    combo->blockSignals(false);
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QString::fromLatin1(kTitle));
    resize(1100, 700);

    const QStringList provinceList = [&] {
        QStringList list;
        for (const char* p : kProvinces)
            list << QString::fromLatin1(p);
        return list;
    }();
    const QStringList cityList = [&] {
        QStringList list;
        for (const char* c : kCities)
            list << QString::fromLatin1(c);
        return list;
    }();
    const QStringList technologyList = [&] {
        QStringList list;
        for (const char* t : kTechnologies)
            list << QString::fromLatin1(t);
        return list;
    }();
    const QStringList operatorList = [&] {
        QStringList list;
        for (const char* o : kOperators)
            list << QString::fromLatin1(o);
        return list;
    }();

    // -- Filter section -----------------------------------------------------
    provinceCombo = makeCombo(this, "provinceCombo", provinceList);
    cityCombo = makeCombo(this, "cityCombo", cityList);
    technologyCombo = makeCombo(this, "technologyCombo", technologyList);
    operatorCombo = makeCombo(this, "operatorCombo", operatorList);

    auto* searchButtonPtr = new QPushButton(tr("Search"), this);
    searchButton = qobject_cast<QPushButton*>(makeStable(searchButtonPtr, "searchButton"));
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
    chartGroupBy = makeCombo(this, "chartGroupBy",
                             {tr("Operator"), tr("Technology")}, false);
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
            this, &MainWindow::onRefresh);
}

void MainWindow::onSearch()
{
    const auto rows = Db::queryFilters(provinceCombo->currentText(), cityCombo->currentText(),
                                       technologyCombo->currentText(),
                                       operatorCombo->currentText());
    signalTable->setRowCount(0);
    for (const auto& row : rows) {
        const int r = signalTable->rowCount();
        signalTable->insertRow(r);
        for (int c = 0; c < row.size() && c < signalTable->columnCount(); ++c)
            signalTable->setItem(r, c, new QTableWidgetItem(row.at(c).toString()));
    }
}

void MainWindow::onRefresh()
{
    onSearch();
}

void MainWindow::onReset()
{
    provinceCombo->setCurrentIndex(0);
    cityCombo->setCurrentIndex(0);
    technologyCombo->setCurrentIndex(0);
    operatorCombo->setCurrentIndex(0);
    onSearch();
}

void MainWindow::onExport()
{
    // Task 5: real export wiring (Db::writeExportCsv + status bar path).
    statusBar()->showMessage(tr("Export not wired yet"));
}

void MainWindow::onProvinceChanged(int /*index*/)
{
    // Task 5: cascading city list (All -> full list, province -> its cities).
    fillCombo(cityCombo, QStringList{});
}
