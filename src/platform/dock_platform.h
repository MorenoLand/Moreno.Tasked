#pragma once

#include <QRect>

class QWindow;

namespace tasked::platform {
void prepareTaskbarSnapshot();
void showStartMenu(const QRect &anchor);
QRect installDock(QWindow *visualWindow, int height);
void uninstallDock();
int runTaskbarGuard(quint32 parentPid);
}
