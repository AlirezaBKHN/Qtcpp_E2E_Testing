// TDD test for Db (database layer). No Qt widgets, no GUI.
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

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

    // 1. init creates + seeds exactly 1500 rows.
    CHECK(Db::init(QString::fromLatin1(kDbPath)));
    CHECK(countRows() == 1500);

    // 2. Re-init on an existing file must not duplicate the seed.
    CHECK(Db::init(QString::fromLatin1(kDbPath)));
    CHECK(countRows() == 1500);

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
        CHECK(samples == 1500);
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
    // No filter -> 1500 rows.
    CHECK(Db::queryFilters(QString(), QString(), QString(), QString()).size() == 1500);
    // Impossible combo -> 0 rows (app never writes outside the map).
    {
        const auto rows = Db::queryFilters("Fars", "Tabriz", QString(), QString());
        CHECK(rows.isEmpty());
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

    // 6. Delete + re-init reseeds to 1500.
    QFile::remove(QString::fromLatin1(kDbPath));
    CHECK(Db::init(QString::fromLatin1(kDbPath)));
    CHECK(countRows() == 1500);

    QFile::remove(QString::fromLatin1(kDbPath));
    if (failures > 0) {
        fprintf(stderr, "db_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("db_test: all checks passed\n");
    return 0;
}
