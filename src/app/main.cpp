#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

#include "Database.h"
#include "AppController.h"

int main(int argc, char *argv[])
{
    // 使用 Basic 样式：无花哨动画/遮罩，在模拟器与低配设备上渲染稳定
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ZhangLvExpense"));
    app.setApplicationVersion(QStringLiteral("0.7.0"));
    app.setOrganizationName(QStringLiteral("ZhangLv"));

    // 数据库落在本应用私有数据目录，不申请任何存储权限
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    const QString dbPath = QDir(dataDir).filePath(QStringLiteral("ledger.sqlite"));

    zl::Database db(dbPath);
    zl::AppController controller(&db);

    QString error;
    if (!controller.initialize(&error)) {
        qCritical() << "数据库初始化失败:" << error;
        return -1;
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &controller);

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.loadFromModule("ZhangLvExpense", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;
    return app.exec();
}
