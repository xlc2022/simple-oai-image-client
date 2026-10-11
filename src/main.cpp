#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("SimpleImageClient"));
    QApplication::setApplicationName(QStringLiteral("SimpleImageClient"));
    QApplication::setApplicationDisplayName(QStringLiteral("Simple Image Client"));
    QApplication::setApplicationVersion(QStringLiteral(PROJECT_VERSION));

    MainWindow window;
    window.show();
    window.promptForSetupIfNeeded();

    return app.exec();
}
