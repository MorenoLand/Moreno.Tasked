#include "dock_platform.h"

#include <QGuiApplication>
#include <QScreen>

#include <algorithm>

QRect tasked::platform::installDock(QWindow *, int height)
{
    const auto area = QGuiApplication::primaryScreen()->availableGeometry();
    const auto width = std::min(1080, std::max(320, area.width() - 32));
    return { area.x() + (area.width() - width) / 2, area.bottom() - height + 1, width, height };
}

void tasked::platform::uninstallDock() {}

int tasked::platform::runTaskbarGuard(quint32) { return 0; }
