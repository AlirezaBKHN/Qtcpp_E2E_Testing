#include "db.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QStringConverter>
#include <QVariant>

#include <random>

namespace {

// Appends the inclusive day-range clauses on sample_time's date part.
// ISO-8601 text compares lexicographically, so string comparison == date
// comparison. An invalid (default) QDate = unbounded on that side.
void appendDateClause(const QDate& from, const QDate& to, QStringList& clauses)
{
    if (from.isValid())
        clauses << QStringLiteral("substr(sample_time, 1, 10) >= :from");
    if (to.isValid())
        clauses << QStringLiteral("substr(sample_time, 1, 10) <= :to");
}

void bindDates(QSqlQuery& query, const QDate& from, const QDate& to)
{
    if (from.isValid())
        query.bindValue(":from", from.toString(Qt::ISODate));
    if (to.isValid())
        query.bindValue(":to", to.toString(Qt::ISODate));
}

// Single source of truth: province -> its cities (plan Global Constraints,
// option (c), 17 cities). cityCombo contents for "All" are every city of
// every province in listed order.
const struct {
    const char* province;
    const char* cities;
} kMap[] = {
    {"Tehran",          "Tehran|Karaj|Qom"},
    {"Isfahan",         "Isfahan|Kashan"},
    {"Fars",            "Shiraz"},
    {"Mazandaran",      "Sari|Amol"},
    {"Khorasan Razavi", "Mashhad|Neyshabur"},
    {"East Azerbaijan", "Tabriz"},
    {"West Azerbaijan", "Urmia"},
    {"Khuzestan",       "Ahvaz|Abadan"},
    {"Gilan",           "Rasht"},
    {"Kerman",          "Kerman|Bam"},
};

const char* kOperators[] = {"MCI", "Irancell", "Rightel", "Shatel Mobile"};
const char* kTechnologies[] = {"2G", "3G", "4G", "5G"};

constexpr int kSeedDays = 7;
constexpr int kRowsPerDay = 1440; // 10080 total: clean 7 trend buckets
constexpr int kSeedRows = kSeedDays * kRowsPerDay;
constexpr uint32_t kFixedSeed = 20241019u;
constexpr int kSeedMinStrength = -120;
constexpr int kSeedMaxStrength = -50;

QStringList splitCities(const char* pipeSeparated)
{
    return QString::fromLatin1(pipeSeparated).split('|');
}

bool tableExists(const QSqlDatabase& db)
{
    QSqlQuery q(db);
    q.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='cells'");
    return q.next();
}

// Seeded demo accounts (owner-confirmed credentials, plaintext, no roles).
const struct {
    const char* username;
    const char* password;
} kSeedUsers[] = {
    {"admin", "admin"},
    {"guest", "guest"},
    {"e2e_user", "e2e123"},
};

QStringList sortedUsernames()
{
    QStringList names;
    QSqlQuery q("SELECT username FROM users");
    while (q.next())
        names << q.value(0).toString();
    names.sort();
    return names;
}

void seed(QSqlDatabase& db)
{
    std::mt19937 rng(kFixedSeed);
    std::uniform_int_distribution<int> provinceDist(0, int(std::size(kMap)) - 1);
    std::uniform_int_distribution<int> operatorDist(0, int(std::size(kOperators)) - 1);
    std::uniform_int_distribution<int> technologyDist(0, int(std::size(kTechnologies)) - 1);
    std::uniform_int_distribution<int> strengthDist(kSeedMinStrength, kSeedMaxStrength);
    std::uniform_int_distribution<int> secondDist(0, 86399);

    // Day-bucketed timestamps: rows 0..1430.. land in day buckets
    // (utcToday-6) .. utcToday so per-day quality always yields exactly
    // kSeedDays entries.
    const QDate today = QDateTime::currentDateTimeUtc().date();
    const QDateTime eraStartUtc(today, QTime(0, 0, 0), Qt::UTC);

    db.transaction();
    QSqlQuery insert(db);
    insert.prepare("INSERT INTO cells (province, city, operator, technology,"
                   " signal_strength, sample_time) VALUES (?, ?, ?, ?, ?, ?)");
    for (int i = 0; i < kSeedRows; ++i) {
        const int p = provinceDist(rng);
        const QStringList cities = splitCities(kMap[p].cities);
        std::uniform_int_distribution<int> cityDist(0, cities.size() - 1);
        const int day = i / kRowsPerDay;
        insert.addBindValue(QString::fromLatin1(kMap[p].province));
        insert.addBindValue(cities.at(cityDist(rng)));
        insert.addBindValue(QString::fromLatin1(kOperators[operatorDist(rng)]));
        insert.addBindValue(QString::fromLatin1(kTechnologies[technologyDist(rng)]));
        insert.addBindValue(strengthDist(rng));
        const QDateTime sampleTime = eraStartUtc.addDays(-6 + day)
                                         .addSecs(secondDist(rng));
        insert.addBindValue(sampleTime.toString(Qt::ISODate));
        insert.exec();
    }
    db.commit();
}

} // namespace

QStringList Db::provinces()
{
    QStringList list;
    for (const auto& entry : kMap)
        list << QString::fromLatin1(entry.province);
    return list;
}

QStringList Db::citiesFor(const QString& province)
{
    for (const auto& entry : kMap)
        if (QString::fromLatin1(entry.province) == province)
            return splitCities(entry.cities);
    QStringList all;
    for (const auto& entry : kMap)
        all << splitCities(entry.cities);
    return all; // empty/All province -> every city
}

bool Db::isProvince(const QString& province)
{
    return provinces().contains(province);
}

QString Db::defaultDatabasePath()
{
    // Test hook: smoke/E2E scratch slots point the canonical path elsewhere
    // (IST_DB_PATH). MainWindow's ctor re-asks the default path - without
    // this override that re-ask would replace a scratch connection back
    // with the working store (the same trap that leaked smoke_reg_success
    // into the committed DB). Unset => unchanged behavior.
    const QString overridePath = qEnvironmentVariable("IST_DB_PATH");
    if (!overridePath.isEmpty())
        return overridePath;
    return QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(QStringLiteral("../../database/signaltracker.db"));
}

bool Db::init(const QString& dbPath)
{
    // Open/ensure the store exactly once per (path). The sign-in dialog
    // ensures the DB at first paint; MainWindow re-asks with the canonical
    // path - the same-path guard keeps that an idempotent no-op instead of
    // a duplicate-connection replacement (data loss risk is also gone: the
    // seeds INSERT OR IGNORE, but a replaced connection is a hidden
    // transaction boundary). A DIFFERENT path still replaces the
    // connection: tests rely on that for scratch databases.
    QSqlDatabase db = QSqlDatabase::database();
    const QString wantPath = QFileInfo(dbPath).absoluteFilePath();
    const QString openPath = db.isValid() && db.isOpen()
                                 ? QFileInfo(db.databaseName()).absoluteFilePath()
                                 : QString();
    if (wantPath != openPath) {
        db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
        db.setDatabaseName(dbPath);
        if (!db.open())
            return false;
    }

    if (!tableExists(db)) {
        QSqlQuery create(db);
        if (!create.exec("CREATE TABLE cells (id INTEGER PRIMARY KEY, province TEXT,"
                         " city TEXT, operator TEXT, technology TEXT,"
                         " signal_strength INTEGER, sample_time TEXT)"))
            return false;
        seed(db);
    }

    if (!QSqlQuery(db).exec("CREATE TABLE IF NOT EXISTS users (username TEXT PRIMARY KEY,"
                            " password TEXT NOT NULL)"))
        return false;
    for (const auto& account : kSeedUsers) {
        // INSERT OR IGNORE: re-init on an existing DB must not duplicate rows.
        QSqlQuery insertQ(db);
        insertQ.prepare("INSERT OR IGNORE INTO users (username, password)"
                        " VALUES (?, ?)");
        insertQ.addBindValue(QString::fromLatin1(account.username));
        insertQ.addBindValue(QString::fromLatin1(account.password));
        if (!insertQ.exec())
            return false;
    }
    return true;
}

QStringList Db::usernames()
{
    return sortedUsernames();
}

bool Db::registerUser(const QString& username, const QString& password)
{
    if (username.isEmpty() || password.isEmpty())
        return false;
    // TEXT PRIMARY KEY is binary-compared in SQLite: case-sensitive exact match.
    if (usernames().contains(username))
        return false;
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isValid())
        return false;
    QSqlQuery insertQ(db);
    insertQ.prepare("INSERT INTO users (username, password) VALUES (?, ?)");
    insertQ.addBindValue(username);
    insertQ.addBindValue(password);
    return insertQ.exec();
}

bool Db::authUser(const QString& username, const QString& password)
{
    if (username.isEmpty() || password.isEmpty())
        return false;
    QSqlQuery q(QSqlDatabase::database());
    q.prepare("SELECT COUNT(*) FROM users WHERE username = :u AND password = :p");
    q.bindValue(":u", username);
    q.bindValue(":p", password);
    q.exec();
    return q.next() && q.value(0).toInt() > 0;
}

bool Db::removeUser(const QString& username)
{
    if (username == QLatin1String("admin"))
        return false; // self-delete guard: admin is never removable
    QSqlQuery q(QSqlDatabase::database());
    q.prepare("DELETE FROM users WHERE username = :u");
    q.bindValue(":u", username);
    q.exec();
    return q.numRowsAffected() > 0;
}

double Db::qualityFrom(double avgDbm)
{
    // The shared 0..100 quality metric (seed domain: -120..-50 dBm maps to
    // 0..100). Single source of truth - UI and Db both call this.
    return (avgDbm + 120.0) / 0.7;
}

double Db::networkQuality(const QString& province, const QString& city,
                          const QString& technology, const QString& operatorName,
                          const QDate& from, const QDate& to)
{
    QStringList clauses;
    if (!province.isEmpty() && province != QLatin1String("All"))
        clauses << "province = :province";
    if (!city.isEmpty() && city != QLatin1String("All"))
        clauses << "city = :city";
    if (!technology.isEmpty() && technology != QLatin1String("All"))
        clauses << "technology = :technology";
    if (!operatorName.isEmpty() && operatorName != QLatin1String("All"))
        clauses << "operator = :operator";
    appendDateClause(from, to, clauses);
    QString sql = QStringLiteral("SELECT AVG(signal_strength) FROM cells");
    if (!clauses.isEmpty())
        sql += QStringLiteral(" WHERE ") + clauses.join(QStringLiteral(" AND "));
    QSqlQuery q(QSqlDatabase::database());
    q.prepare(sql);
    if (clauses.contains("province = :province"))
        q.bindValue(":province", province);
    if (clauses.contains("city = :city"))
        q.bindValue(":city", city);
    if (clauses.contains("technology = :technology"))
        q.bindValue(":technology", technology);
    if (clauses.contains("operator = :operator"))
        q.bindValue(":operator", operatorName);
    bindDates(q, from, to);
    q.exec();
    // AVG over zero matching rows comes back NULL (the filtered empty
    // state): the quality metric is 0.0, never qualityFrom(0)'s 171.4%.
    if (!q.next() || q.value(0).isNull())
        return 0.0;
    return qualityFrom(q.value(0).toDouble());
}

int Db::cellCount(const QDate& from, const QDate& to)
{
    QStringList clauses;
    appendDateClause(from, to, clauses);
    QString sql = QStringLiteral("SELECT COUNT(*) FROM cells");
    if (!clauses.isEmpty())
        sql += QStringLiteral(" WHERE ") + clauses.join(QStringLiteral(" AND "));
    QSqlQuery q(QSqlDatabase::database());
    q.prepare(sql);
    bindDates(q, from, to);
    q.exec();
    return q.next() ? q.value(0).toInt() : 0;
}

QVector<LabelAvg> Db::dailyQuality(const QString& groupColumn,
                                  const QString& groupValue,
                                  const QDate& from, const QDate& to)
{
    QVector<LabelAvg> points;
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isValid())
        return points;

    // Only the whitelisted group columns reach %1 (SQL-injection guard).
    if (!groupColumn.isEmpty()
        && groupColumn != QLatin1String("province")
        && groupColumn != QLatin1String("operator")
        && groupColumn != QLatin1String("technology"))
        return points;

    bool filtered = !groupValue.isEmpty() && !groupColumn.isEmpty();
    QString sql = QStringLiteral(
                      "SELECT substr(sample_time, 1, 10) AS day,"
                      " AVG(signal_strength) FROM cells");
    QStringList clauses;
    if (filtered)
        clauses << QStringLiteral("%1 = :v").arg(groupColumn);
    appendDateClause(from, to, clauses);
    if (!clauses.isEmpty())
        sql += QStringLiteral(" WHERE ") + clauses.join(QStringLiteral(" AND "));
    sql += QLatin1String(" GROUP BY day ORDER BY day");

    QSqlQuery q(db);
    q.prepare(sql);
    if (filtered)
        q.bindValue(":v", groupValue);
    bindDates(q, from, to);
    q.exec();
    while (q.next())
        points.append({q.value(0).toString(),
                       qualityFrom(q.value(1).toDouble())});
    return points;
}

QVector<QStringList> Db::worstCells(int limit, const QString& province,
                                    const QString& city, const QString& technology,
                                    const QString& operatorName,
                                    const QDate& from, const QDate& to)
{
    QVector<QStringList> rows;
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isValid())
        return rows;

    QStringList clauses;
    clauses << QStringLiteral("signal_strength < -102.5");
    if (!province.isEmpty() && province != QLatin1String("All"))
        clauses << "province = :province";
    if (!city.isEmpty() && city != QLatin1String("All"))
        clauses << "city = :city";
    if (!technology.isEmpty() && technology != QLatin1String("All"))
        clauses << "technology = :technology";
    if (!operatorName.isEmpty() && operatorName != QLatin1String("All"))
        clauses << "operator = :operator";
    appendDateClause(from, to, clauses);

    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT id, province, city, operator, technology,"
                             " signal_strength, sample_time FROM cells WHERE ")
              + clauses.join(QStringLiteral(" AND "))
              + QStringLiteral(" ORDER BY signal_strength ASC, id ASC LIMIT :l"));
    q.bindValue(":l", limit);
    if (clauses.contains("province = :province"))
        q.bindValue(":province", province);
    if (clauses.contains("city = :city"))
        q.bindValue(":city", city);
    if (clauses.contains("technology = :technology"))
        q.bindValue(":technology", technology);
    if (clauses.contains("operator = :operator"))
        q.bindValue(":operator", operatorName);
    bindDates(q, from, to);
    q.exec();
    while (q.next()) {
        QStringList row;
        for (int c = 0; c < 7; ++c)
            row << q.value(c).toString();
        rows.append(row);
    }
    return rows;
}

FilterBindings Db::queryFilters(const QString& province, const QString& city,
                                const QString& technology, const QString& operatorName,
                                const QDate& from, const QDate& to)
{
    FilterBindings rows;
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isValid())
        return rows;

    QStringList clauses;
    if (!province.isEmpty() && province != QLatin1String("All"))
        clauses << "province = :province";
    if (!city.isEmpty() && city != QLatin1String("All"))
        clauses << "city = :city";
    if (!technology.isEmpty() && technology != QLatin1String("All"))
        clauses << "technology = :technology";
    if (!operatorName.isEmpty() && operatorName != QLatin1String("All"))
        clauses << "operator = :operator";
    appendDateClause(from, to, clauses);

    QString sql = "SELECT id, province, city, operator, technology, signal_strength,"
                  " sample_time FROM cells";
    if (!clauses.isEmpty())
        sql += " WHERE " + clauses.join(" AND ");
    // Owner directive (spec 2026-10-20): province groups, id-ordered
    // within - a total order (id is INTEGER PRIMARY KEY) pagination
    // can slice deterministically. writeExportCsv iterates queryFilters,
    // so the CSV export order matches the table order automatically.
    sql += " ORDER BY province, id";

    QSqlQuery q(db);
    q.prepare(sql);
    if (clauses.contains("province = :province"))
        q.bindValue(":province", province);
    if (clauses.contains("city = :city"))
        q.bindValue(":city", city);
    if (clauses.contains("technology = :technology"))
        q.bindValue(":technology", technology);
    if (clauses.contains("operator = :operator"))
        q.bindValue(":operator", operatorName);
    bindDates(q, from, to);
    q.exec();

    while (q.next())
        rows.append(QVariantList{q.value(0), q.value(1), q.value(2),
                                 q.value(3), q.value(4), q.value(5), q.value(6)});
    return rows;
}

QVector<LabelAvg> Db::avgBy(const QString& groupColumn, const QString& province,
                            const QString& city, const QString& technology,
                            const QString& operatorName,
                            const QDate& from, const QDate& to)
{
    QVector<LabelAvg> points;
    if (groupColumn != QLatin1String("operator")
        && groupColumn != QLatin1String("technology")
        && groupColumn != QLatin1String("province"))
        return points;

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isValid())
        return points;

    // Only the two whitelisted labels reach %1 (validated above).
    QString sql = QStringLiteral(
                      "SELECT %1, AVG(signal_strength) FROM cells").arg(groupColumn);
    QStringList clauses;
    if (!province.isEmpty() && province != QLatin1String("All"))
        clauses << "province = :province";
    if (!city.isEmpty() && city != QLatin1String("All"))
        clauses << "city = :city";
    if (!technology.isEmpty() && technology != QLatin1String("All"))
        clauses << "technology = :technology";
    if (!operatorName.isEmpty() && operatorName != QLatin1String("All"))
        clauses << "operator = :operator";
    appendDateClause(from, to, clauses);
    if (!clauses.isEmpty())
        sql += " WHERE " + clauses.join(" AND ");
    sql += " GROUP BY " + groupColumn;

    QSqlQuery q(db);
    q.prepare(sql);
    q.bindValue(":province", province);
    q.bindValue(":city", city);
    q.bindValue(":technology", technology);
    q.bindValue(":operator", operatorName);
    bindDates(q, from, to);
    q.exec();
    while (q.next())
        points.append({q.value(0).toString(), q.value(1).toDouble()});
    return points;
}

QVector<QPair<QString, int>> Db::countBy(const QString& groupColumn,
                                         const QString& province,
                                         const QString& city,
                                         const QString& technology,
                                         const QString& operatorName,
                                         const QDate& from, const QDate& to)
{
    QVector<QPair<QString, int>> points;
    if (groupColumn != QLatin1String("operator")
        && groupColumn != QLatin1String("technology")
        && groupColumn != QLatin1String("province"))
        return points;

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isValid())
        return points;

    // Only the whitelisted labels reach %1 (validated above).
    QString sql = QStringLiteral("SELECT %1, COUNT(*) FROM cells").arg(groupColumn);
    QStringList clauses;
    if (!province.isEmpty() && province != QLatin1String("All"))
        clauses << "province = :province";
    if (!city.isEmpty() && city != QLatin1String("All"))
        clauses << "city = :city";
    if (!technology.isEmpty() && technology != QLatin1String("All"))
        clauses << "technology = :technology";
    if (!operatorName.isEmpty() && operatorName != QLatin1String("All"))
        clauses << "operator = :operator";
    appendDateClause(from, to, clauses);
    if (!clauses.isEmpty())
        sql += " WHERE " + clauses.join(" AND ");
    sql += " GROUP BY " + groupColumn + " ORDER BY " + groupColumn + " ASC";

    QSqlQuery q(db);
    q.prepare(sql);
    q.bindValue(":province", province);
    q.bindValue(":city", city);
    q.bindValue(":technology", technology);
    q.bindValue(":operator", operatorName);
    bindDates(q, from, to);
    q.exec();
    while (q.next())
        points.append({q.value(0).toString(), q.value(1).toInt()});
    return points;
}

QPair<QDate, QDate> Db::dataEra()
{
    QSqlQuery q(QSqlDatabase::database());
    q.prepare("SELECT MIN(substr(sample_time, 1, 10)),"
              " MAX(substr(sample_time, 1, 10)) FROM cells");
    q.exec();
    if (!q.next())
        return {QDate(), QDate()};
    return {QDate::fromString(q.value(0).toString(), Qt::ISODate),
            QDate::fromString(q.value(1).toString(), Qt::ISODate)};
}

bool Db::writeExportCsv(const QString& path, const QString& province,
                        const QString& city, const QString& technology,
                        const QString& operatorName)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << "id,province,city,operator,technology,signal_strength,timestamp\n";
    auto csvCell = [](const QString& value) {
        if (value.contains(',') || value.contains('"') || value.contains('\n'))
            return QLatin1Char('"') + QString(value).replace("\"", "\"\"") + QLatin1Char('"');
        return value;
    };
    for (const auto& row : queryFilters(province, city, technology, operatorName)) {
        QStringList cells;
        for (const auto& field : row)
            cells << csvCell(field.toString());
        out << cells.join(',') << '\n';
    }
    return true;
}
