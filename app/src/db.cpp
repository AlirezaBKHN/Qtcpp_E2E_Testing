#include "db.h"

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

constexpr int kSeedRows = 1500;
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

void seed(QSqlDatabase& db)
{
    std::mt19937 rng(kFixedSeed);
    std::uniform_int_distribution<int> provinceDist(0, int(std::size(kMap)) - 1);
    std::uniform_int_distribution<int> operatorDist(0, int(std::size(kOperators)) - 1);
    std::uniform_int_distribution<int> technologyDist(0, int(std::size(kTechnologies)) - 1);
    std::uniform_int_distribution<int> strengthDist(kSeedMinStrength, kSeedMaxStrength);

    QDateTime sampleTime = QDateTime::currentDateTimeUtc();

    db.transaction();
    QSqlQuery insert(db);
    insert.prepare("INSERT INTO cells (province, city, operator, technology,"
                   " signal_strength, sample_time) VALUES (?, ?, ?, ?, ?, ?)");
    for (int i = 0; i < kSeedRows; ++i) {
        const int p = provinceDist(rng);
        const QStringList cities = splitCities(kMap[p].cities);
        std::uniform_int_distribution<int> cityDist(0, cities.size() - 1);
        insert.addBindValue(QString::fromLatin1(kMap[p].province));
        insert.addBindValue(cities.at(cityDist(rng)));
        insert.addBindValue(QString::fromLatin1(kOperators[operatorDist(rng)]));
        insert.addBindValue(QString::fromLatin1(kTechnologies[technologyDist(rng)]));
        insert.addBindValue(strengthDist(rng));
        sampleTime = sampleTime.addSecs(-60);
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

bool Db::init(const QString& dbPath)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    db.setDatabaseName(dbPath);
    if (!db.open())
        return false;

    if (!tableExists(db)) {
        QSqlQuery create(db);
        if (!create.exec("CREATE TABLE cells (id INTEGER PRIMARY KEY, province TEXT,"
                         " city TEXT, operator TEXT, technology TEXT,"
                         " signal_strength INTEGER, sample_time TEXT)"))
            return false;
        seed(db);
    }
    return true;
}

FilterBindings Db::queryFilters(const QString& province, const QString& city,
                                const QString& technology, const QString& operatorName)
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

    QString sql = "SELECT id, province, city, operator, technology, signal_strength,"
                  " sample_time FROM cells";
    if (!clauses.isEmpty())
        sql += " WHERE " + clauses.join(" AND ");
    sql += " ORDER BY province, city, operator, technology";

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
    q.exec();

    while (q.next())
        rows.append(QVariantList{q.value(0), q.value(1), q.value(2),
                                 q.value(3), q.value(4), q.value(5), q.value(6)});
    return rows;
}

QVector<LabelAvg> Db::avgBy(const QString& groupColumn, const QString& province,
                            const QString& city, const QString& technology,
                            const QString& operatorName)
{    QVector<LabelAvg> points;
    if (groupColumn != QLatin1String("operator") && groupColumn != QLatin1String("technology"))
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
    if (!clauses.isEmpty())
        sql += " WHERE " + clauses.join(" AND ");
    sql += " GROUP BY " + groupColumn;

    QSqlQuery q(db);
    q.prepare(sql);
    q.bindValue(":province", province);
    q.bindValue(":city", city);
    q.bindValue(":technology", technology);
    q.bindValue(":operator", operatorName);
    q.exec();
    while (q.next())
        points.append({q.value(0).toString(), q.value(1).toDouble()});
    return points;
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
