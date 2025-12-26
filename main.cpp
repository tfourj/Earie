#include <QApplication>
#include <QIcon>

#include "AppController.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName(QStringLiteral("Earie"));
    app.setOrganizationName(QStringLiteral("Earie"));

    AppController controller;
    if (!controller.init()) {
        return 1;
    }

    return app.exec();
}
