#include <QApplication>

#include "mainwindow.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Iran Signal Tracker");
    QApplication::setOrganizationName("IranSignalTrackerDemo");

    MainWindow window;
    window.show();
    return app.exec();
}
