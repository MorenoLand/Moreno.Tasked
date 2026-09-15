#include "dock_platform.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QWindow>

#include <windows.h>
#include <shellapi.h>

#include <algorithm>

namespace {
constexpr UINT callbackMessage = WM_APP + 0x47;
QWindow *reservation = nullptr;
QWindow *visual = nullptr;
QAbstractNativeEventFilter *eventFilter = nullptr;
int dockHeight = 92;

HWND nativeHandle(QWindow *window) { return reinterpret_cast<HWND>(window->winId()); }
void positionDock();

class DockEventFilter final : public QAbstractNativeEventFilter
{
public:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) override
    {
        if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") return false;
        const auto *msg = static_cast<MSG *>(message);
        if (reservation && msg->hwnd == nativeHandle(reservation) && msg->message == callbackMessage) positionDock();
        return false;
    }
};

void positionDock()
{
    if (!reservation) return;
    APPBARDATA data{};
    data.cbSize = sizeof(data);
    data.hWnd = nativeHandle(reservation);
    data.uEdge = ABE_BOTTOM;
    MONITORINFO monitor{ sizeof(monitor) };
    GetMonitorInfoW(MonitorFromWindow(data.hWnd, MONITOR_DEFAULTTONEAREST), &monitor);
    data.rc = monitor.rcMonitor;
    SHAppBarMessage(ABM_QUERYPOS, &data);
    data.rc.top = data.rc.bottom - dockHeight;
    SHAppBarMessage(ABM_SETPOS, &data);
    reservation->setGeometry(data.rc.left, data.rc.top, data.rc.right - data.rc.left, data.rc.bottom - data.rc.top);
    if (!visual) return;
    const auto width = (std::min)(960, (std::max)(320, reservation->width() - 32));
    visual->setGeometry(data.rc.left + (reservation->width() - width) / 2, data.rc.bottom - visual->height(), width, visual->height());
}
}

QRect tasked::platform::installDock(QWindow *visualWindow, int height)
{
    visual = visualWindow;
    dockHeight = height;
    reservation = new QWindow;
    reservation->setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowTransparentForInput);
    reservation->setOpacity(0.0);
    reservation->show();
    APPBARDATA data{};
    data.cbSize = sizeof(data);
    data.hWnd = nativeHandle(reservation);
    data.uCallbackMessage = callbackMessage;
    SHAppBarMessage(ABM_NEW, &data);
    eventFilter = new DockEventFilter;
    qApp->installNativeEventFilter(eventFilter);
    positionDock();
    return visual->geometry();
}

void tasked::platform::uninstallDock()
{
    if (reservation) {
        APPBARDATA data{};
        data.cbSize = sizeof(data);
        data.hWnd = nativeHandle(reservation);
        SHAppBarMessage(ABM_REMOVE, &data);
    }
    if (eventFilter) {
        qApp->removeNativeEventFilter(eventFilter);
        delete eventFilter;
        eventFilter = nullptr;
    }
    delete reservation;
    reservation = nullptr;
    visual = nullptr;
}
