#include <QApplication>
#include <QIcon>
#include <QQuickStyle>

#include "AppController.h"

int main(int argc, char *argv[])
{
    // Required for custom Slider background/handle styling in QML.
    // The native Windows style does not support these customizations.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

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
