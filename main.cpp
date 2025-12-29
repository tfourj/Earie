#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QIcon>
#include <QMessageBox>
#include <QQuickStyle>
#include <QSharedMemory>
#include <QTextStream>

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
    
    if (QFile::exists(QStringLiteral("log.enable"))) {
        if (QFile::exists(QStringLiteral("earie.log"))) {
            if (QFile::exists(QStringLiteral("earie.old.log")))
                QFile::remove(QStringLiteral("earie.old.log"));
            QFile::rename(QStringLiteral("earie.log"), QStringLiteral("earie.old.log"));
        }
        {
            QFile logFile(QStringLiteral("earie.log"));
            if (logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&logFile);
                out << "logging started at " << QDateTime::currentDateTime().toString() << "\n";
                logFile.close();
            }
        }
        qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &, const QString &msg) {
            QFile logFile(QStringLiteral("earie.log"));
            if (logFile.open(QIODevice::Append | QIODevice::Text)) {
                QTextStream out(&logFile);
                out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << " " << msg << "\n";
                logFile.close();
            }
        });
    }
    
    QSharedMemory instanceGuard(QStringLiteral("Earie.SingleInstance"));
    if (!instanceGuard.create(1)) {
        QMessageBox::warning(nullptr,
                             QStringLiteral("Earie"),
                             QStringLiteral("Earie is already running."));
        return 0;
    }

    AppController controller;
    if (!controller.init()) {
        return 1;
    }

    return app.exec();
}
