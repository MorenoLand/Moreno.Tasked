#include "dock_platform.h"

#include <QGuiApplication>
#include <QScreen>

#include <algorithm>

void tasked::platform::prepareTaskbarSnapshot() {}
void tasked::platform::showStartMenu(const QRect &) {}
void tasked::platform::showRunDialog(const QRect &) {}
void tasked::platform::showSystemTrayFlyout(const QRect &) {}

QRect tasked::platform::installDock(QWindow *, int height, int position)
{
    const auto area = QGuiApplication::primaryScreen()->availableGeometry();
    if (position > 1) return { area.x() + (position == 3 ? area.width() - height - 12 : 12), area.y() + 12, height, std::max(320, area.height() - 32) };
    const auto width = std::min(1080, std::max(320, area.width() - 32));
    return { area.x() + (area.width() - width) / 2, position == 1 ? area.y() + 12 : area.bottom() - height + 1, width, height };
}

void tasked::platform::setDockPosition(int) {}

void tasked::platform::uninstallDock() {}

int tasked::platform::runTaskbarGuard(quint32) { return 0; }
