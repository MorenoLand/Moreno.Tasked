#include "app_launcher.h"

#include <QDesktopServices>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

bool AppLauncher::launch(const QString &target)
{
    if (target.contains(':')) return QDesktopServices::openUrl(QUrl(target));
    const auto executable = QFileInfo::exists(target) ? target : QStandardPaths::findExecutable(target);
    return !executable.isEmpty() && QProcess::startDetached(executable, {});
}
