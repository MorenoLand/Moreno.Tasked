#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QWindow>

#include "app_launcher.h"
#include "app_settings.h"
#include "platform/dock_platform.h"
#include "platform/tray_icon_provider.h"
#include "platform/windows_shell.h"
#include "shell_icon_provider.h"

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    if (argc >= 3 && QString::fromLocal8Bit(argv[1]) == "--tasked-taskbar-guard") return tasked::platform::runTaskbarGuard(QString::fromLocal8Bit(argv[2]).toUInt());
#endif
    QCoreApplication::setOrganizationName("Tasked");
    QCoreApplication::setApplicationName("Tasked");
    QGuiApplication app(argc, argv);
    tasked::platform::prepareTaskbarSnapshot();
    QQmlApplicationEngine engine;
    AppLauncher launcher;
    TaskedSettings taskedSettings;
    RunningAppsModel runningApps;
    TrayModel trayIcons;
    engine.addImageProvider("shell", new ShellIconProvider);
    engine.addImageProvider("tray", new TrayIconProvider(&trayIcons));
    engine.rootContext()->setContextProperty("launcher", &launcher);
    engine.rootContext()->setContextProperty("taskedSettings", &taskedSettings);
    engine.rootContext()->setContextProperty("runningApps", &runningApps);
    engine.rootContext()->setContextProperty("trayIcons", &trayIcons);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("Tasked", "Main");
    if (engine.rootObjects().isEmpty()) return -1;
    auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
    if (!window) return -1;
    window->setGeometry(tasked::platform::installDock(window, 92));
    window->show();
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [] { tasked::platform::uninstallDock(); });
    return app.exec();
}
