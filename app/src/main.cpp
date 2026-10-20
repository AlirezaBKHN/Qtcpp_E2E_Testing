#include <QApplication>

#include "mainwindow.h"
#include "signin_dialog.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("Iran Signal Tracker");
    QApplication::setOrganizationName("IranSignalTrackerDemo");

    // Register-first gate: nothing of the app is reachable until a login
    // succeeds. Logout reopens the dialog; closing the app window exits.
    for (;;) {
        SignInDialog dialog;
        dialog.show();
        if (dialog.exec() != QDialog::Accepted)
            return 0;

        MainWindow window(dialog.user());
        window.show();
        app.exec();
        if (window.shouldExit())
            return 0;
    }
}
