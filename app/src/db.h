#pragma once

#include <QDate>
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
    // Opens (creating + seeding if absent) signaltracker.db. Calling with
    // the path the default connection is already open on is an idempotent
    // no-op (the sign-in dialog ensures the store at first paint; the main
    // window re-asks with the canonical path). A different path replaces
    // the connection (scratch-db test isolation).
    static bool init(const QString& dbPath);

    // The canonical DB path for the deployed app layout
    // (<exeDir>/../../database/signaltracker.db).
    static QString defaultDatabasePath();

    // Stable domain map; single source of truth for the UI and tests.
    static QStringList provinces();
    static QStringList citiesFor(const QString& province);
    static bool isProvince(const QString& province);

    // Filtered rows; empty argument = no filter on that column. The date
    // bounds are inclusive day ranges on sample_time's date part; an
    // invalid (default) QDate = unbounded on that side.
    static FilterBindings queryFilters(const QString& province, const QString& city,
                                       const QString& technology,
                                       const QString& operatorName,
                                       const QDate& from = {},
                                       const QDate& to = {});

    // (label, avg(signal_strength)) series for the chart. The group column
    // is whitelisted (province/operator/technology); empty filter args are no-op.
    static QVector<LabelAvg> avgBy(const QString& groupColumn, const QString& province,
                                   const QString& city, const QString& technology,
                                   const QString& operatorName,
                                   const QDate& from = {},
                                   const QDate& to = {});

    // (label, COUNT(*)) series, same whitelisting and filter semantics as
    // avgBy - the slice data for the share charts.
    static QVector<QPair<QString, int>> countBy(const QString& groupColumn,
                                                const QString& province,
                                                const QString& city,
                                                const QString& technology,
                                                const QString& operatorName,
                                                const QDate& from = {},
                                                const QDate& to = {});

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

    // Dashboard aggregates. qualityFrom is THE definition of the shared
    // 0..100 quality metric ((avg dBm + 120) / 0.7); every quality number
    // in UI or tests derives through it - never inline the transform.
    static double qualityFrom(double avgDbm);
    // The filtered average quality (0..100 metric): column filters + date
    // range, all defaulted to unbounded so existing callers are unchanged.
    static double networkQuality(const QString& province = {},
                                 const QString& city = {},
                                 const QString& technology = {},
                                 const QString& operatorName = {},
                                 const QDate& from = {},
                                 const QDate& to = {});
    static int cellCount(const QDate& from = {},
                         const QDate& to = {});
    // (YYYY-MM-DD, quality) pairs ascending; groupColumn must be one of
    // province/operator/technology and groupValue filters that column -
    // empty groupValue = the whole network. Always exactly 7 buckets on the
    // seeded era (Task 2). Date bounds as in queryFilters.
    static QVector<LabelAvg> dailyQuality(const QString& groupColumn,
                                         const QString& groupValue,
                                         const QDate& from = {},
                                         const QDate& to = {});
    // Weakest rows first (strength < -102.5 dBm = quality below 25%),
    // capped at limit; column order matches the main table. Accepts the
    // same filter set as queryFilters (dashboard/report filters reach it).
    static QVector<QStringList> worstCells(int limit = 8,
                                           const QString& province = {},
                                           const QString& city = {},
                                           const QString& technology = {},
                                           const QString& operatorName = {},
                                           const QDate& from = {},
                                           const QDate& to = {});
    // (MIN, MAX) of the cells table's date part as QDates (invalid when the
    // table is empty) - the default range for the date filter widgets.
    static QPair<QDate, QDate> dataEra();
};
