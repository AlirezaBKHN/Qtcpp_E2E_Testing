#pragma once

#include <QString>
#include <QVariantList>
#include <QVector>

using LabelAvg = QPair<QString, double>;
using FilterBindings = QVector<QVariantList>;

// SQLite access for the cells table. Implemented in Task 4; Task 3 ships
// compiling stubs so the UI shell can link.
class Db
{
public:
    // Opens (creating + seeding if absent) signaltracker.db.
    static bool init(const QString& dbPath);

    // Stable domain map; single source of truth for the UI and tests.
    static QStringList provinces();
    static QStringList citiesFor(const QString& province);
    static bool isProvince(const QString& province);

    // Filtered rows; empty argument = no filter on that column.
    static FilterBindings queryFilters(const QString& province, const QString& city,
                                       const QString& technology,
                                       const QString& operatorName);

    // (label, avg(signal_strength)) series for the chart.
    static QVector<LabelAvg> avgBy(const QString& groupColumn, const QString& province,
                                   const QString& city, const QString& technology,
                                   const QString& operatorName);
};
