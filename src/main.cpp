#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QWindow>
#ifdef Q_OS_WIN
#include <windows.h>
#include "resources/resource.h"
#endif

#include "app_launcher.h"
#include "app_settings.h"
#include "extensions_model.h"
#include "preview_controller.h"
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
    const QIcon appIcon(QStringLiteral(":/Tasked-icon.png"));
    app.setWindowIcon(appIcon);
    tasked::platform::prepareTaskbarSnapshot();
    QQmlApplicationEngine engine;
    AppLauncher launcher;
    TaskedSettings taskedSettings;
    ExtensionModel extensions;
    PreviewController previewController;
    RunningAppsModel runningApps;
    TrayModel trayIcons;
    TrayFilterModel dockTrayIcons(&trayIcons, false);
    TrayFilterModel overflowTrayIcons(&trayIcons, true);
    engine.addImageProvider("shell", new ShellIconProvider);
    engine.addImageProvider("tray", new TrayIconProvider(&trayIcons));
    engine.rootContext()->setContextProperty("launcher", &launcher);
    engine.rootContext()->setContextProperty("taskedSettings", &taskedSettings);
    engine.rootContext()->setContextProperty("extensions", &extensions);
    engine.rootContext()->setContextProperty("previewController", &previewController);
    engine.rootContext()->setContextProperty("runningApps", &runningApps);
    engine.rootContext()->setContextProperty("trayIcons", &trayIcons);
    engine.rootContext()->setContextProperty("dockTrayIcons", &dockTrayIcons);
    engine.rootContext()->setContextProperty("overflowTrayIcons", &overflowTrayIcons);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.loadFromModule("Tasked", "Main");
    if (engine.rootObjects().isEmpty()) return -1;
    auto *window = qobject_cast<QWindow *>(engine.rootObjects().constFirst());
    if (!window) return -1;
    window->setIcon(appIcon);
    window->setGeometry(tasked::platform::installDock(window, 92));
    window->show();
#ifdef Q_OS_WIN
    const auto nativeWindow = reinterpret_cast<HWND>(window->winId());
    const auto module = GetModuleHandleW(nullptr);
    const auto largeIcon = reinterpret_cast<HICON>(LoadImageW(module, MAKEINTRESOURCEW(IDI_TASKED_ICON), IMAGE_ICON, 32, 32, LR_DEFAULTSIZE));
    const auto smallIcon = reinterpret_cast<HICON>(LoadImageW(module, MAKEINTRESOURCEW(IDI_TASKED_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTSIZE));
    if (largeIcon) SendMessageW(nativeWindow, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(largeIcon));
    if (smallIcon) SendMessageW(nativeWindow, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
#endif
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [] { tasked::platform::uninstallDock(); });
    return app.exec();
}
