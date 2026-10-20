// TDD test for Db (database layer). No Qt widgets, no GUI.
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTextStream>
#include <QVariant>

#include <cmath>

#include "../src/db.h"

static int failures = 0;
#define CHECK(expr)                                                             \
    do {                                                                        \
        if (!(expr)) {                                                          \
            ++failures;                                                          \
            fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);      \
        }                                                                       \
    } while (0)

static const char* kDbPath = "dbtest_cell.db";
static const char* kProvinces[] = {"Tehran",         "Isfahan", "Fars",
                                   "Mazandaran",     "Khorasan Razavi", "East Azerbaijan",
                                   "West Azerbaijan", "Khuzestan",   "Gilan",
                                   "Kerman"};
static const char* kOperators[] = {"MCI", "Irancell", "Rightel", "Shatel Mobile"};
static const char* kTechnologies[] = {"2G", "3G", "4G", "5G"};

static long long countRows()
{
    QSqlQuery q("SELECT COUNT(*) FROM cells");
    q.next();
    return q.value(0).toLongLong();
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QFile::remove(QString::fromLatin1(kDbPath));

    // 1. init creates + seeds exactly 10080 rows (7 days x 1440).
    CHECK(Db::init(QString::fromLatin1(kDbPath)));
    CHECK(countRows() == 10080);

    // 2. Re-init on an existing file must not duplicate the seed.
    CHECK(Db::init(QString::fromLatin1(kDbPath)));
    CHECK(countRows() == 10080);

    // 3. Row content legality + province->city mapping.
    {
        QSet<QString> provinces, operators, technologies;
        QSet<int> strengths;
        int samples = 0;
        QSqlQuery q("SELECT province, city, operator, technology, signal_strength FROM cells");
        while (q.next()) {
            ++samples;
            const QString province = q.value(0).toString();
            const QString city = q.value(1).toString();
            const QString oper = q.value(2).toString();
            const QString tech = q.value(3).toString();
            const int strength = q.value(4).toInt();
            provinces.insert(province);
            operators.insert(oper);
            technologies.insert(tech);
            CHECK(strength >= -120 && strength <= -50);
            const QStringList legalCities = Db::citiesFor(province);
            CHECK(!legalCities.isEmpty());
            CHECK(legalCities.contains(city));
        }
        CHECK(samples == 10080);
        for (const char* p : kProvinces)
            CHECK(provinces.contains(QString::fromLatin1(p)));
        for (const char* o : kOperators)
            CHECK(operators.contains(QString::fromLatin1(o)));
        for (const char* t : kTechnologies)
            CHECK(technologies.contains(QString::fromLatin1(t)));
        CHECK(provinces.contains("Qom") == false); // Qom is a city, not a province
    }

    // 4. queryFilters narrows by province + city.
    {
        const auto rows = Db::queryFilters("Fars", "Shiraz", QString(), QString());
        CHECK(!rows.isEmpty());
        for (const auto& row : rows)
            CHECK(row.at(2).toString() == "Shiraz");
    }
    // No filter -> 10080 rows.
    CHECK(Db::queryFilters(QString(), QString(), QString(), QString()).size() == 10080);
    // Impossible combo -> 0 rows (app never writes outside the map).
    {
        const auto rows = Db::queryFilters("Fars", "Tabriz", QString(), QString());
        CHECK(rows.isEmpty());
    }
    // queryFilters order (spec 2026-10-20): provinces in contiguous ascending
    // blocks, ids strictly ascending within each block - the total order
    // pagination slices (page k = rows [(k-1)*size, k*size)).
    {
        const auto rows = Db::queryFilters(QString(), QString(), QString(), QString());
        CHECK(!rows.isEmpty());
        QString prevProvince;
        int prevId = -1;
        bool firstRow = true;
        for (const auto& row : rows) {
            const QString province = row.at(1).toString();
            const int id = row.at(0).toInt();
            if (firstRow || province != prevProvince) {
                if (!firstRow)
                    CHECK(province > prevProvince); // contiguous alphabetical blocks
                prevProvince = province;
                prevId = -1;
                firstRow = false;
            } else {
                CHECK(id > prevId); // ascending ids within the block
            }
            prevId = id;
        }
    }

    // 5. avgBy groups to exactly operator-size with sane averages.
    {
        const auto points = Db::avgBy("operator", QString(), QString(), QString(), QString());
        CHECK(points.size() == 4);
        for (const auto& point : points)
            CHECK(point.second >= -120.0 && point.second <= -50.0);
    }
    {
        const auto points = Db::avgBy("technology", "Tehran", QString(), QString(), QString());
        CHECK(points.size() >= 1 && points.size() <= 4);
    }

    // 6. Delete + re-init reseeds to 10080.
    QFile::remove(QString::fromLatin1(kDbPath));
    CHECK(Db::init(QString::fromLatin1(kDbPath)));
    CHECK(countRows() == 10080);

    // 7. writeExportCsv writes header + one line per filtered row.
    {
        const QString path = "export_test.csv";
        QFile::remove(path);
        const auto rows = Db::queryFilters("Fars", QString(), QString(), QString());
        CHECK(!rows.isEmpty());
        CHECK(Db::writeExportCsv(path, "Fars", QString(), QString(), QString()));
        CHECK(QFile::exists(path));
        QFile file(path);
        CHECK(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QTextStream in(&file);
        CHECK(in.readLine() ==
              "id,province,city,operator,technology,signal_strength,timestamp");
        int dataLines = 0;
        while (!in.atEnd()) {
            if (!in.readLine().trimmed().isEmpty())
                ++dataLines;
        }
        CHECK(dataLines == rows.size());
        file.close();
        QFile::remove(path);
    }

    // 8. Zero-row export still creates a header-only file.
    {
        const QString path = "export_test_zero.csv";
        QFile::remove(path);
        CHECK(Db::writeExportCsv(path, "Fars", "Tabriz", QString(), QString()));
        CHECK(QFile::exists(path));
        QFile file(path);
        CHECK(file.open(QIODevice::ReadOnly | QIODevice::Text));
        QTextStream in(&file);
        const QString first = in.readLine();
        CHECK(first ==
              "id,province,city,operator,technology,signal_strength,timestamp");
        CHECK(in.readLine().trimmed().isEmpty());
        file.close();
        QFile::remove(path);
    }

    // 9. Timestamps span exactly 7 UTC date buckets (trend granularity).
    // Per-province counts are pinned to the regenerated committed DB
    // (app_dbgen, std::mt19937(20241019)) so seed drift is caught.
    {
        const QHash<QString, int> kProvinceCounts{
            {"Tehran", 1025}, {"Mazandaran", 978}, {"Fars", 1023},
            {"Isfahan", 1021}, {"Khuzestan", 1015}, {"Khorasan Razavi", 1019},
        };
        for (auto it = kProvinceCounts.constBegin(); it != kProvinceCounts.constEnd(); ++it) {
            QSqlQuery q(QSqlDatabase::database());
            q.prepare("SELECT COUNT(*) FROM cells WHERE province = :p");
            q.bindValue(":p", it.key());
            q.exec();
            q.next();
            CHECK(q.value(0).toInt() == it.value());
        }
    }
    {
        QSet<QString> dates;
        QSqlQuery q("SELECT DISTINCT substr(sample_time, 1, 10) FROM cells");
        while (q.next())
            dates.insert(q.value(0).toString());
        CHECK(dates.size() == 7);
        // Every row sits inside a 7-day window ending "today" (UTC).
        const QDate today = QDateTime::currentDateTimeUtc().date();
        for (const QString& dateStr : dates) {
            CHECK(dateStr.size() == 10);
            CHECK(QDate::fromString(dateStr, Qt::ISODate) > today.addDays(-7));
            CHECK(QDate::fromString(dateStr, Qt::ISODate) <= today);
        }
    }

    // 10. users table: seeded credentials + auth/register/remove/list APIs.
    {
        // Seeded users exist and are sorted.
        const QStringList names = Db::usernames();
        CHECK(names.size() == 3);
        CHECK(names.at(0) <= names.at(1) && names.at(1) <= names.at(2));
        CHECK(names.contains("admin"));
        CHECK(names.contains("guest"));
        CHECK(names.contains("e2e_user"));

        // Seeded credentials authenticate.
        CHECK(Db::authUser("admin", "admin"));
        CHECK(Db::authUser("guest", "guest"));
        CHECK(Db::authUser("e2e_user", "e2e123"));

        // Wrong password / unknown username fail on exact match.
        CHECK(!Db::authUser("admin", "wrong"));
        CHECK(!Db::authUser("nobody", "admin"));
        CHECK(Db::authUser("Admin", "admin") == false); // case-sensitive username
        CHECK(Db::authUser("admin", "Admin") == false); // case-sensitive password

        // registerUser: duplicate username (case-sensitive exact match) rejected.
        CHECK(!Db::registerUser("admin", "anything"));
        // Empty username / empty password rejected, no mutation.
        CHECK(!Db::registerUser("", "pw"));
        CHECK(!Db::registerUser("tempuser", ""));
        CHECK(!Db::usernames().contains("tempuser"));

        // registerUser success + sorted insertion.
        CHECK(Db::registerUser("tempuser", "temppw"));
        CHECK(Db::authUser("tempuser", "temppw"));
        {
            const QStringList after = Db::usernames();
            CHECK(after.size() == 4);
            CHECK(after.at(0) <= after.at(1) && after.at(1) <= after.at(2) &&
                  after.at(2) <= after.at(3));
        }
        CHECK(!Db::registerUser("tempuser", "other")); // duplicate

        // removeUser: admin protected, unknown rejected, real user removed.
        CHECK(!Db::removeUser("admin"));
        CHECK(!Db::removeUser("NoSuchUser"));
        CHECK(Db::removeUser("tempuser"));
        CHECK(!Db::usernames().contains("tempuser"));
        CHECK(!Db::authUser("tempuser", "temppw"));
    }

    // 11. Dashboard aggregates (Task 5): cross-checked against independent
    // straight SQL formulations of the same questions.
    {
        // networkQuality = (AVG(dBm) + 120) / 0.7 over the whole table.
        {
            QSqlQuery q("SELECT AVG(signal_strength) FROM cells");
            q.next();
            const double expected = (q.value(0).toDouble() + 120.0) / 0.7;
            CHECK(qAbs(Db::networkQuality() - expected) < 1e-9);
        }
        CHECK(Db::cellCount() == 10080);

        // Whole-network daily quality: exactly 7 ascending day buckets,
        // each matching a plain GROUP BY on the date substring.
        {
            const auto points = Db::dailyQuality(QString(), QString());
            CHECK(points.size() == 7);
            for (int i = 1; i < points.size(); ++i)
                CHECK(points.at(i - 1).first < points.at(i).first);
            QSqlQuery q("SELECT substr(sample_time,1,10) AS day, AVG(signal_strength)"
                        " FROM cells GROUP BY day ORDER BY day");
            int i = 0;
            while (q.next() && i < points.size()) {
                CHECK(q.value(0).toString() == points.at(i).first);
                const double expected = (q.value(1).toDouble() + 120.0) / 0.7;
                CHECK(qAbs(expected - points.at(i).second) < 1e-9);
                ++i;
            }
            CHECK(i == 7 && !q.next());
        }
        // Grouped by province.
        {
            const auto points =
                Db::dailyQuality(QStringLiteral("province"), QStringLiteral("Tehran"));
            CHECK(points.size() == 7);
            QSqlQuery grouped(QSqlDatabase::database());
            grouped.prepare("SELECT substr(sample_time,1,10) AS day, AVG(signal_strength)"
                            " FROM cells WHERE province = :p GROUP BY day ORDER BY day");
            grouped.bindValue(":p", QString::fromLatin1("Tehran"));
            grouped.exec();
            int i = 0;
            while (grouped.next() && i < points.size()) {
                CHECK(grouped.value(0).toString() == points.at(i).first);
                const double expected = (grouped.value(1).toDouble() + 120.0) / 0.7;
                CHECK(qAbs(expected - points.at(i).second) < 1e-9);
                ++i;
            }
            CHECK(i == 7 && !grouped.next());
        }
        // Whitelist guard.
        CHECK(Db::dailyQuality(QStringLiteral("hack"), QStringLiteral("x")).isEmpty());

        // worstCells: weakest first, capped, integer strengths <= -103
        // (strictly below the -102.5 quality-25% line).
        {
            const auto worst = Db::worstCells(8);
            CHECK(worst.size() == 8);
            QSqlQuery q("SELECT id, province, city, operator, technology,"
                        " signal_strength, sample_time FROM cells"
                        " WHERE signal_strength < -102.5"
                        " ORDER BY signal_strength ASC, id ASC LIMIT 8");
            for (const auto& row : worst) {
                CHECK(row.size() == 7);
                CHECK(row.at(5).toInt() <= -103);
            }
            int i = 0;
            while (q.next() && i < worst.size()) {
                CHECK(q.value(0).toString() == worst.at(i).at(0));
                CHECK(q.value(5).toString() == worst.at(i).at(5));
                ++i;
            }
            CHECK(i == 8 && !q.next());
            for (int j = 1; j < worst.size(); ++j) {
                CHECK(worst.at(j).at(5).toInt() >= worst.at(j - 1).at(5).toInt());
            }
        }
    }

    QFile::remove(QString::fromLatin1(kDbPath));
    if (failures > 0) {
        fprintf(stderr, "db_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("db_test: all checks passed\n");
    return 0;
}
