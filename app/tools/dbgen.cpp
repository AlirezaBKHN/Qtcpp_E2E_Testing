// One-shot generator for the committed database/signaltracker.db artifact.
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QString>

#include "../src/db.h"

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        fprintf(stderr, "usage: app_dbgen <db-path>\n");
        return 2;
    }
    const QString path = QString::fromLocal8Bit(argv[1]);
    if (!Db::init(path)) {
        fprintf(stderr, "Db::init failed for '%s'\n", qPrintable(path));
        // Replicate the open to surface the driver-level error.
        QSqlDatabase probe = QSqlDatabase::addDatabase("QSQLITE");
        probe.setDatabaseName(path);
        if (!probe.open())
            fprintf(stderr, "driver open error: %s\n",
                    qPrintable(probe.lastError().text()));
        return 1;
    }
    return 0;
}
