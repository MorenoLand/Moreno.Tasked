#include "dock_platform.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QRect>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <QWindow>

#include <windows.h>
#include <shellapi.h>
#include <UIAutomation.h>
#include <oleauto.h>

#include <algorithm>
#include <array>
#include <cwchar>

namespace {
constexpr UINT callbackMessage = WM_APP + 0x47;
QWindow *reservation = nullptr;
QWindow *visual = nullptr;
QAbstractNativeEventFilter *eventFilter = nullptr;
QVector<HWND> hiddenTaskbars;
int dockHeight = 92;
int dockPosition = 0;
UINT taskbarCreatedMessage = 0;
bool takeover = false;
QTimer *startPositionTimer = nullptr;
QRect startAnchor;
int startPositionMisses = 0;
bool startMenuOpen = false;
QTimer *runPositionTimer = nullptr;
QRect runAnchor;
int runPositionMisses = 0;
int runPositionFrames = 0;
QTimer *trayFlyoutTimer = nullptr;
QRect trayFlyoutAnchor;
int trayFlyoutMisses = 0;
bool trayFlyoutFallbackSent = false;
QTimer *fullscreenTimer = nullptr;
bool fullscreenHidden = false;

HWND nativeHandle(QWindow *window) { return reinterpret_cast<HWND>(window->winId()); }
void positionDock();
bool isShellExperience(HWND window);
bool isDesktopWindow(HWND window);
void sendShortcut(WORD modifier, WORD key);
void sendQuickSettingsHotkey();

void updateFullscreenVisibility()
{
    if (!visual) return;
    const auto foreground = GetForegroundWindow();
    bool fullscreen = false;
    if (foreground && foreground != nativeHandle(visual) && foreground != nativeHandle(reservation) && !isShellExperience(foreground) && !isDesktopWindow(foreground)) {
        RECT bounds{};
        const auto monitor = MonitorFromWindow(foreground, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{ sizeof(info) };
        if (GetWindowRect(foreground, &bounds) && GetMonitorInfoW(monitor, &info)) fullscreen = bounds.left <= info.rcMonitor.left && bounds.top <= info.rcMonitor.top && bounds.right >= info.rcMonitor.right && bounds.bottom >= info.rcMonitor.bottom;
    }
    if (fullscreen == fullscreenHidden) return;
    fullscreenHidden = fullscreen;
    ShowWindow(nativeHandle(visual), fullscreen ? SW_HIDE : SW_SHOWNA);
}

bool isTaskbar(HWND window)
{
    wchar_t className[64]{};
    GetClassNameW(window, className, ARRAYSIZE(className));
    return wcscmp(className, L"Shell_TrayWnd") == 0 || wcscmp(className, L"Shell_SecondaryTrayWnd") == 0;
}

bool isDesktopWindow(HWND window)
{
    wchar_t className[64]{};
    GetClassNameW(window, className, ARRAYSIZE(className));
    return wcscmp(className, L"Progman") == 0 || wcscmp(className, L"WorkerW") == 0 || isTaskbar(window);
}

BOOL CALLBACK hideTaskbarWindow(HWND window, LPARAM)
{
    if (isTaskbar(window) && IsWindowVisible(window)) {
        hiddenTaskbars.append(window);
        ShowWindow(window, SW_HIDE);
    }
    return TRUE;
}

BOOL CALLBACK restoreTaskbarWindow(HWND window, LPARAM)
{
    if (isTaskbar(window)) ShowWindow(window, SW_SHOW);
    return TRUE;
}

void hideTaskbars()
{
    hiddenTaskbars.clear();
    EnumWindows(hideTaskbarWindow, 0);
}

void restoreTaskbars()
{
    for (const auto window : hiddenTaskbars) if (IsWindow(window)) ShowWindow(window, SW_SHOW);
    hiddenTaskbars.clear();
}

void restoreAllTaskbars() { EnumWindows(restoreTaskbarWindow, 0); }

bool isStartExperience(HWND window)
{
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) return false;
    std::array<wchar_t, MAX_PATH> path{};
    DWORD length = static_cast<DWORD>(path.size());
    const auto ok = QueryFullProcessImageNameW(process, 0, path.data(), &length);
    CloseHandle(process);
    return ok && _wcsicmp(wcsrchr(path.data(), L'\\') ? wcsrchr(path.data(), L'\\') + 1 : path.data(), L"StartMenuExperienceHost.exe") == 0;
}

bool isShellExperience(HWND window)
{
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) return false;
    std::array<wchar_t, MAX_PATH> path{};
    DWORD length = static_cast<DWORD>(path.size());
    const auto ok = QueryFullProcessImageNameW(process, 0, path.data(), &length);
    CloseHandle(process);
    const auto name = wcsrchr(path.data(), L'\\');
    const auto executable = name ? name + 1 : path.data();
    return ok && (_wcsicmp(executable, L"ShellExperienceHost.exe") == 0 || _wcsicmp(executable, L"ShellHost.exe") == 0 || _wcsicmp(executable, L"StartMenuExperienceHost.exe") == 0);
}

BOOL CALLBACK findStartPopup(HWND window, LPARAM parameter)
{
    if (!IsWindowVisible(window)) return TRUE;
    wchar_t className[64]{};
    wchar_t title[64]{};
    GetClassNameW(window, className, ARRAYSIZE(className));
    GetWindowTextW(window, title, ARRAYSIZE(title));
    const auto startExperience = isStartExperience(window);
    const auto startWindow = startExperience && wcscmp(className, L"Windows.UI.Core.CoreWindow") == 0 && wcscmp(title, L"Start") == 0;
    if (startWindow || (startExperience && wcscmp(className, L"Xaml_WindowedPopupClass") == 0 && wcscmp(title, L"PopupHost") == 0)) {
        *reinterpret_cast<HWND *>(parameter) = window;
        return FALSE;
    }
    return TRUE;
}

QRect startPanelBounds(HWND host)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IUIAutomation *uiAutomation = nullptr;
    IUIAutomationElement *root = nullptr;
    IUIAutomationCondition *condition = nullptr;
    IUIAutomationElement *element = nullptr;
    QRect result;
    if (SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&uiAutomation))) && uiAutomation && SUCCEEDED(uiAutomation->ElementFromHandle(host, &root)) && root) {
        VARIANT value{};
        value.vt = VT_BSTR;
        value.bstrVal = SysAllocString(L"StartMenuPinnedList");
        if (value.bstrVal && SUCCEEDED(uiAutomation->CreatePropertyCondition(UIA_AutomationIdPropertyId, value, &condition)) && condition && SUCCEEDED(root->FindFirst(TreeScope_Descendants, condition, &element)) && element) {
            BOOL offscreen = TRUE;
            RECT bounds{};
            if (SUCCEEDED(element->get_CurrentIsOffscreen(&offscreen)) && !offscreen && SUCCEEDED(element->get_CurrentBoundingRectangle(&bounds)) && bounds.right > bounds.left && bounds.bottom > bounds.top) {
                const auto monitor = MonitorFromRect(&bounds, MONITOR_DEFAULTTONEAREST);
                MONITORINFO info{ sizeof(info) };
                if (GetMonitorInfoW(monitor, &info) && bounds.right > info.rcMonitor.left && bounds.left < info.rcMonitor.right && bounds.bottom > info.rcMonitor.top && bounds.top < info.rcMonitor.bottom) result = QRect(bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top);
            }
        }
        if (value.bstrVal) SysFreeString(value.bstrVal);
    }
    if (element) element->Release();
    if (condition) condition->Release();
    if (root) root->Release();
    if (uiAutomation) uiAutomation->Release();
    if (initialized == S_OK || initialized == S_FALSE) CoUninitialize();
    return result;
}

BOOL CALLBACK findStartPopupChild(HWND window, LPARAM parameter) { return findStartPopup(window, parameter); }

BOOL CALLBACK findStartPopupChildren(HWND window, LPARAM parameter)
{
    EnumChildWindows(window, findStartPopupChild, parameter);
    return *reinterpret_cast<HWND *>(parameter) == nullptr;
}

HWND startPopup()
{
    if (const auto result = FindWindowW(L"Windows.UI.Core.CoreWindow", L"Start"); result && IsWindowVisible(result) && isStartExperience(result)) return result;
    if (const auto result = FindWindowW(L"Xaml_WindowedPopupClass", L"PopupHost"); result && IsWindowVisible(result) && isStartExperience(result)) return result;
    HWND result = nullptr;
    EnumWindows(findStartPopup, reinterpret_cast<LPARAM>(&result));
    if (!result) EnumWindows(findStartPopupChildren, reinterpret_cast<LPARAM>(&result));
    return result;
}

BOOL CALLBACK findTrayFlyout(HWND window, LPARAM parameter)
{
    if (!IsWindowVisible(window)) return TRUE;
    wchar_t className[64]{};
    wchar_t title[128]{};
    GetClassNameW(window, className, ARRAYSIZE(className));
    GetWindowTextW(window, title, ARRAYSIZE(title));
    const auto popup = isShellExperience(window) && wcscmp(className, L"Xaml_WindowedPopupClass") == 0 && (wcscmp(title, L"PopupHost") == 0 || title[0] == L'\0');
    const auto core = isShellExperience(window) && wcscmp(className, L"Windows.UI.Core.CoreWindow") == 0 && (_wcsicmp(title, L"Quick Settings") == 0 || _wcsicmp(title, L"Network") == 0 || _wcsicmp(title, L"Volume") == 0);
    if (popup || core) {
        *reinterpret_cast<HWND *>(parameter) = window;
        return FALSE;
    }
    return TRUE;
}

BOOL CALLBACK findTrayFlyoutChild(HWND window, LPARAM parameter) { return findTrayFlyout(window, parameter); }
BOOL CALLBACK findTrayFlyoutChildren(HWND window, LPARAM parameter)
{
    EnumChildWindows(window, findTrayFlyoutChild, parameter);
    return *reinterpret_cast<HWND *>(parameter) == nullptr;
}

HWND systemTrayFlyout()
{
    HWND result = nullptr;
    EnumWindows(findTrayFlyout, reinterpret_cast<LPARAM>(&result));
    if (!result) EnumWindows(findTrayFlyoutChildren, reinterpret_cast<LPARAM>(&result));
    return result;
}

bool positionTrayFlyout(const QRect &anchor)
{
    const auto popup = systemTrayFlyout();
    if (!popup) return false;
    RECT current{};
    if (!GetWindowRect(popup, &current)) return false;
    const auto monitor = MonitorFromPoint({ anchor.center().x(), anchor.top() }, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{ sizeof(info) };
    GetMonitorInfoW(monitor, &info);
    const auto width = current.right - current.left;
    const auto height = current.bottom - current.top;
    if (width <= 0 || height <= 0) return false;
    auto x = anchor.right() - width;
    x = (std::max)(info.rcWork.left + 8, (std::min)(x, info.rcWork.right - width - 8));
    auto y = anchor.top() - height - 8;
    if (y < info.rcWork.top + 8) y = anchor.bottom() + 8;
    POINT position{ x, y };
    if (GetWindowLongPtrW(popup, GWL_STYLE) & WS_CHILD) {
        const auto parent = GetParent(popup);
        if (parent) ScreenToClient(parent, &position);
    }
    return SetWindowPos(popup, HWND_TOP, position.x, position.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
}

void trayFlyoutMonitor(const QRect &anchor)
{
    if (!trayFlyoutTimer) {
        trayFlyoutTimer = new QTimer(qApp);
        trayFlyoutTimer->setInterval(16);
        QObject::connect(trayFlyoutTimer, &QTimer::timeout, [] { if (positionTrayFlyout(trayFlyoutAnchor)) trayFlyoutMisses = 0; else if (++trayFlyoutMisses == 8 && !trayFlyoutFallbackSent) { sendQuickSettingsHotkey(); trayFlyoutFallbackSent = true; } else if (trayFlyoutMisses > 60) trayFlyoutTimer->stop(); });
    }
    trayFlyoutAnchor = anchor;
    trayFlyoutMisses = 0;
    trayFlyoutFallbackSent = false;
    trayFlyoutTimer->start();
}

bool positionStartPopup(const QRect &anchor)
{
    const auto popup = startPopup();
    if (!popup) return false;
    RECT current{};
    if (!GetWindowRect(popup, &current)) return false;
    wchar_t className[64]{};
    wchar_t title[64]{};
    GetClassNameW(popup, className, ARRAYSIZE(className));
    GetWindowTextW(popup, title, ARRAYSIZE(title));
    if (wcscmp(className, L"Windows.UI.Core.CoreWindow") == 0 && wcscmp(title, L"Start") == 0) {
        const auto panel = startPanelBounds(popup);
        if (!panel.isValid() && startPositionMisses > 30 && GetForegroundWindow() != popup) return false;
        const auto scale = static_cast<int>(GetDpiForWindow(popup)) / 96.0;
        const auto panelWidth = panel.isValid() ? panel.width() : static_cast<int>(642 * scale);
        const auto panelLeft = panel.isValid() ? panel.left() : current.left + static_cast<int>(12 * scale);
        const auto monitor = MonitorFromPoint({ anchor.center().x(), anchor.top() }, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{ sizeof(info) };
        GetMonitorInfoW(monitor, &info);
        const auto minimumX = static_cast<int>(info.rcWork.left + 8);
        const auto maximumX = static_cast<int>(info.rcWork.right - panelWidth - 8);
        auto panelX = anchor.center().x() - panelWidth / 2;
        panelX = (std::max)(minimumX, (std::min)(panelX, maximumX));
        const auto x = current.left + panelX - panelLeft;
        const auto moved = SetWindowPos(popup, nullptr, x, current.top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSENDCHANGING);
        return moved;
    }
    const auto monitor = MonitorFromPoint({ anchor.center().x(), anchor.top() }, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{ sizeof(info) };
    GetMonitorInfoW(monitor, &info);
    const auto width = current.right - current.left;
    const auto height = current.bottom - current.top;
    const auto minimumX = info.rcWork.left + 8;
    const auto maximumX = info.rcWork.right - width - 8;
    auto x = anchor.center().x() - width / 2;
    x = (std::max)(minimumX, (std::min)(x, maximumX));
    auto y = anchor.top() - height - 10;
    if (y < info.rcWork.top + 8) y = anchor.bottom() + 10;
    POINT position{ x, y };
    if (GetWindowLongPtrW(popup, GWL_STYLE) & WS_CHILD) {
        const auto parent = GetParent(popup);
        if (parent) ScreenToClient(parent, &position);
    }
    return SetWindowPos(popup, HWND_TOP, position.x, position.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
}

bool isRunDialog(HWND window)
{
    if (!window || !IsWindowVisible(window)) return false;
    wchar_t className[64]{};
    wchar_t title[64]{};
    GetClassNameW(window, className, ARRAYSIZE(className));
    GetWindowTextW(window, title, ARRAYSIZE(title));
    return wcscmp(className, L"#32770") == 0 && _wcsicmp(title, L"Run") == 0;
}

BOOL CALLBACK findRunDialog(HWND window, LPARAM parameter)
{
    if (isRunDialog(window)) {
        *reinterpret_cast<HWND *>(parameter) = window;
        return FALSE;
    }
    return TRUE;
}

HWND runDialog()
{
    if (const auto result = FindWindowW(L"#32770", L"Run"); isRunDialog(result)) return result;
    HWND result = nullptr;
    EnumWindows(findRunDialog, reinterpret_cast<LPARAM>(&result));
    return result;
}

bool positionRunDialog(const QRect &anchor)
{
    const auto dialog = runDialog();
    if (!dialog) return false;
    RECT current{};
    if (!GetWindowRect(dialog, &current)) return false;
    const auto width = current.right - current.left;
    const auto height = current.bottom - current.top;
    if (width <= 0 || height <= 0) return false;
    const auto monitor = MonitorFromPoint({ anchor.center().x(), anchor.center().y() }, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{ sizeof(info) };
    GetMonitorInfoW(monitor, &info);
    auto x = anchor.center().x() - width / 2;
    auto y = anchor.top() - height - 12;
    if (dockPosition == 1) y = anchor.bottom() + 12;
    else if (dockPosition == 2) {
        x = anchor.right() + 12;
        y = anchor.center().y() - height / 2;
    } else if (dockPosition == 3) {
        x = anchor.left() - width - 12;
        y = anchor.center().y() - height / 2;
    }
    x = (std::max)(info.rcWork.left + 8, (std::min)(x, info.rcWork.right - width - 8));
    y = (std::max)(info.rcWork.top + 8, (std::min)(y, info.rcWork.bottom - height - 8));
    return SetWindowPos(dialog, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

bool startMenuIsOpen() { const auto popup = startPopup(); return popup && IsWindowVisible(popup) && startPanelBounds(popup).isValid(); }
void sendKey(WORD key)
{
    INPUT input[2]{};
    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = key;
    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wVk = key;
    input[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, input, sizeof(INPUT));
}

void sendShortcut(WORD modifier, WORD key)
{
    INPUT input[4]{};
    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = modifier;
    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wVk = key;
    input[2] = input[1];
    input[2].ki.dwFlags = KEYEVENTF_KEYUP;
    input[3] = input[0];
    input[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, input, sizeof(INPUT));
}

void sendQuickSettingsHotkey() { sendShortcut(VK_LWIN, 'A'); }

void openQuickSettings()
{
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", L"ms-actioncenter:controlcenter/true", nullptr, nullptr, SW_SHOWNOACTIVATE));
    if (result <= 32) {
        sendQuickSettingsHotkey();
        trayFlyoutFallbackSent = true;
    }
}

void startPositionMonitor(const QRect &anchor)
{
    if (!startPositionTimer) {
        startPositionTimer = new QTimer(qApp);
        startPositionTimer->setInterval(16);
        QObject::connect(startPositionTimer, &QTimer::timeout, [] { const auto positioned = positionStartPopup(startAnchor); if (startMenuIsOpen()) { startMenuOpen = true; startPositionMisses = 0; } else if (!positioned || ++startPositionMisses > 30) { startMenuOpen = false; startPositionTimer->stop(); } });
    }
    startAnchor = anchor;
    startPositionMisses = 0;
    startPositionTimer->start();
}

void runPositionMonitor(const QRect &anchor)
{
    if (!runPositionTimer) {
        runPositionTimer = new QTimer(qApp);
        runPositionTimer->setInterval(16);
        QObject::connect(runPositionTimer, &QTimer::timeout, [] { if (positionRunDialog(runAnchor)) { runPositionMisses = 0; if (++runPositionFrames > 45) runPositionTimer->stop(); } else if (++runPositionMisses > 60) runPositionTimer->stop(); });
    }
    runAnchor = anchor;
    runPositionMisses = 0;
    runPositionFrames = 0;
    runPositionTimer->start();
}

bool startTaskbarGuard()
{
    return QProcess::startDetached(QCoreApplication::applicationFilePath(), { "--tasked-taskbar-guard", QString::number(GetCurrentProcessId()) });
}

class DockEventFilter final : public QAbstractNativeEventFilter
{
public:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *) override
    {
        if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") return false;
        const auto *msg = static_cast<MSG *>(message);
        if (takeover && taskbarCreatedMessage && msg->message == taskbarCreatedMessage) {
            hideTaskbars();
            positionDock();
            return false;
        }
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
    data.uEdge = dockPosition == 0 ? ABE_BOTTOM : dockPosition == 1 ? ABE_TOP : dockPosition == 2 ? ABE_LEFT : ABE_RIGHT;
    MONITORINFO monitor{ sizeof(monitor) };
    GetMonitorInfoW(MonitorFromWindow(data.hWnd, MONITOR_DEFAULTTONEAREST), &monitor);
    data.rc = monitor.rcMonitor;
    SHAppBarMessage(ABM_QUERYPOS, &data);
    if (dockPosition == 0) {
        data.rc.bottom = monitor.rcMonitor.bottom;
        data.rc.top = data.rc.bottom - dockHeight;
    } else if (dockPosition == 1) {
        data.rc.top = monitor.rcMonitor.top;
        data.rc.bottom = data.rc.top + dockHeight;
    } else if (dockPosition == 2) {
        data.rc.left = monitor.rcMonitor.left;
        data.rc.right = data.rc.left + dockHeight;
    } else {
        data.rc.right = monitor.rcMonitor.right;
        data.rc.left = data.rc.right - dockHeight;
    }
    SHAppBarMessage(ABM_SETPOS, &data);
    reservation->setGeometry(data.rc.left, data.rc.top, data.rc.right - data.rc.left, data.rc.bottom - data.rc.top);
    if (!visual) return;
    const auto horizontal = dockPosition < 2;
    const auto maximumLength = (std::max)(320, (horizontal ? reservation->width() : reservation->height()) - 32);
    const auto currentLength = horizontal ? visual->width() : visual->height();
    const auto length = (std::min)(maximumLength, (std::max)(320, currentLength));
    if (horizontal) visual->setGeometry(data.rc.left + (reservation->width() - length) / 2, dockPosition == 1 ? monitor.rcMonitor.top + 12 : monitor.rcMonitor.bottom - visual->height() - 12, length, visual->height());
    else visual->setGeometry(dockPosition == 2 ? monitor.rcMonitor.left + 12 : monitor.rcMonitor.right - visual->width() - 12, monitor.rcMonitor.top + (reservation->height() - length) / 2, visual->width(), length);
}
}

void tasked::platform::prepareTaskbarSnapshot() { restoreAllTaskbars(); }
void tasked::platform::showStartMenu(const QRect &anchor)
{
    if (startMenuOpen || startMenuIsOpen()) {
        if (startPositionTimer) startPositionTimer->stop();
        sendKey(VK_ESCAPE);
        startMenuOpen = false;
        return;
    }
    sendKey(VK_LWIN);
    startMenuOpen = true;
    startPositionMonitor(anchor);
}

void tasked::platform::showRunDialog(const QRect &anchor)
{
    sendShortcut(VK_LWIN, 'R');
    runPositionMonitor(anchor);
}

void tasked::platform::showSystemTrayFlyout(const QRect &anchor) { trayFlyoutMonitor(anchor); openQuickSettings(); }

QRect tasked::platform::installDock(QWindow *visualWindow, int height, int position)
{
    visual = visualWindow;
    dockHeight = height;
    dockPosition = (std::max)(0, (std::min)(3, position));
    taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
    if (startTaskbarGuard()) {
        hideTaskbars();
        takeover = true;
    }
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
    QObject::connect(visual, &QWindow::widthChanged, visual, [](int) { positionDock(); });
    QObject::connect(visual, &QWindow::heightChanged, visual, [](int) { positionDock(); });
    fullscreenTimer = new QTimer(qApp);
    fullscreenTimer->setInterval(250);
    QObject::connect(fullscreenTimer, &QTimer::timeout, updateFullscreenVisibility);
    fullscreenTimer->start();
    positionDock();
    return visual->geometry();
}

void tasked::platform::setDockPosition(int position)
{
    dockPosition = (std::max)(0, (std::min)(3, position));
    positionDock();
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
    if (fullscreenTimer) {
        fullscreenTimer->stop();
        delete fullscreenTimer;
        fullscreenTimer = nullptr;
    }
    if (fullscreenHidden && visual) ShowWindow(nativeHandle(visual), SW_SHOWNA);
    fullscreenHidden = false;
    if (takeover) {
        restoreTaskbars();
        takeover = false;
    }
    delete reservation;
    reservation = nullptr;
    visual = nullptr;
}

int tasked::platform::runTaskbarGuard(quint32 parentPid)
{
    const auto parent = OpenProcess(SYNCHRONIZE, FALSE, parentPid);
    if (parent) {
        WaitForSingleObject(parent, INFINITE);
        CloseHandle(parent);
    }
    restoreAllTaskbars();
    return 0;
}
