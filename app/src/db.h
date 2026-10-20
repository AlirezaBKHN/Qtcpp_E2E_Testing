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

    // Writes the current (filtered) view as CSV: one header line + one line
    // per row. Returns false if the file cannot be created. Zero-row filters
    // still produce a header-only file.
    static bool writeExportCsv(const QString& path, const QString& province,
                               const QString& city, const QString& technology,
                               const QString& operatorName);

    // Auth layer (plaintext demo-grade; owner ruling). usernames() is the
    // sorted user list. Registration requires a non-empty username not
    // already present (case-sensitive exact match) and a non-empty password.
    static QStringList usernames();
    static bool registerUser(const QString& username, const QString& password);
    static bool authUser(const QString& username, const QString& password);
    // The literal 'admin' account can never be removed; unknown users fail too.
    static bool removeUser(const QString& username);

    // Dashboard aggregates (quality = (avg dBm + 120) / 0.7 on a 0..100 scale).
    static double networkQuality();
    static int cellCount();
    // (YYYY-MM-DD, quality) pairs ascending; groupColumn must be one of
    // province/operator/technology and groupValue filters that column -
    // empty groupValue = the whole network. Always exactly 7 buckets on the
    // seeded era (Task 2).
    static QVector<LabelAvg> dailyQuality(const QString& groupColumn,
                                         const QString& groupValue);
    // Weakest rows first (strength < -102.5 dBm = quality below 25%),
    // capped at limit; column order matches the main table.
    static QVector<QStringList> worstCells(int limit = 8);
};
