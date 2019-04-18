#include "mainwindow.h"

#include <QApplication>
#include <QDir>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Needed for QSettings.
    app.setOrganizationName("CodeThesis");
    app.setApplicationName("QFtpServer");

    // Show the main window.
    MainWindow mainWindow;
    mainWindow.setOrientation(MainWindow::ScreenOrientationAuto);
    mainWindow.showExpanded();

    // start server
    // second argv is the path of config file.
    QString config = (argc > 1) ? argv[1] : "";

    if (config.isEmpty())
        config = QDir::currentPath() + "/config.json";

    mainWindow.loadSettings(config);
    mainWindow.startServer();

    return app.exec();
}
