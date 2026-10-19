#pragma once

#include <QComboBox>
#include <QMainWindow>
#include <QPushButton>
#include <QStatusBar>
#include <QTableWidget>

class QChartView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);

private slots:
    void onSearch();
    void onRefresh();
    void onReset();
    void onExport();
    void onProvinceChanged(int index);

private:
    void refreshChart();
    QComboBox* provinceCombo = nullptr;
    QComboBox* cityCombo = nullptr;
    QComboBox* technologyCombo = nullptr;
    QComboBox* operatorCombo = nullptr;
    QComboBox* chartGroupBy = nullptr;

    QPushButton* searchButton = nullptr;
    QPushButton* refreshButton = nullptr;
    QPushButton* resetButton = nullptr;
    QPushButton* exportButton = nullptr;

    QTableWidget* signalTable = nullptr;
    QChartView* signalChart = nullptr;
};
