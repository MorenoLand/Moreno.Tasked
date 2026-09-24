#pragma once

#include <QRect>

class QWindow;

namespace tasked::platform {
void prepareTaskbarSnapshot();
void showStartMenu(const QRect &anchor);
void showRunDialog(const QRect &anchor);
void showSystemTrayFlyout(const QRect &anchor);
QRect installDock(QWindow *visualWindow, int height, int position);
void setDockPosition(int position);
void uninstallDock();
int runTaskbarGuard(quint32 parentPid, const QRect &originalWorkArea);
}
