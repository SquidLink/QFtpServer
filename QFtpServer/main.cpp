#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Needed for QSettings.
    app.setOrganizationName("CodeThesis");
    app.setApplicationName("QFtpServer");

    // Show the main window.
    MainWindow mainWindow;

    // second argv is the path of config file.
    if (argc > 2) {
        QString name(argv[1]);
        mainWindow.setConfigPath(name);
    }

    mainWindow.setOrientation(MainWindow::ScreenOrientationAuto);
    mainWindow.showExpanded();

    return app.exec();
}
