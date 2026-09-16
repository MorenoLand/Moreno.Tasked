#pragma once

#include <QRect>

class QWindow;

namespace tasked::platform {
QRect installDock(QWindow *visualWindow, int height);
void uninstallDock();
int runTaskbarGuard(quint32 parentPid);
}
