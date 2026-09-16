#include "app_launcher.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#include "platform/dock_platform.h"

bool AppLauncher::launch(const QString &target)
{
    if (target.contains(':')) return QDesktopServices::openUrl(QUrl(target));
    const auto executable = QFileInfo::exists(target) ? target : QStandardPaths::findExecutable(target);
    return !executable.isEmpty() && QProcess::startDetached(executable, {});
}

void AppLauncher::showStartMenu(int x, int y, int width, int height) { tasked::platform::showStartMenu({ x, y, width, height }); }
void AppLauncher::showSystemTrayFlyout(int x, int y, int width, int height) { tasked::platform::showSystemTrayFlyout({ x, y, width, height }); }
