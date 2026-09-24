#include "dock_platform.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QRect>
#include <QProcess>
#include <QScreen>
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
#include <unordered_map>

namespace {
constexpr UINT callbackMessage = WM_APP + 0x47;
QWindow *reservation = nullptr;
QWindow *visual = nullptr;
HWND appbarHandle = nullptr;
bool appbarRegistered = false;
QAbstractNativeEventFilter *eventFilter = nullptr;
QVector<HWND> hiddenTaskbars;
int dockHeight = 92;
int dockPosition = 0;
bool positioningDock = false;
std::unordered_map<HMONITOR, RECT> originalWorkAreas;
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
QTimer *fullscreenTimer = nullptr;
bool fullscreenHidden = false;

struct MaximizedWindowContext { HMONITOR monitor; RECT workArea; };

BOOL CALLBACK refitMaximizedWindow(HWND window, LPARAM parameter)
{
    const auto &context = *reinterpret_cast<MaximizedWindowContext *>(parameter);
    if (!IsWindowVisible(window) || !IsZoomed(window) || MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST) != context.monitor) return TRUE;
    RECT current{};
    if (GetWindowRect(window, &current) && EqualRect(&current, &context.workArea)) return TRUE;
    SetWindowPos(window, nullptr, context.workArea.left, context.workArea.top, context.workArea.right - context.workArea.left, context.workArea.bottom - context.workArea.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_ASYNCWINDOWPOS);
    return TRUE;
}

void refitMaximizedWindows(HMONITOR monitor, const RECT &workArea) { MaximizedWindowContext context{ monitor, workArea }; EnumWindows(refitMaximizedWindow, reinterpret_cast<LPARAM>(&context)); }

HWND nativeHandle(QWindow *window) { return reinterpret_cast<HWND>(window->winId()); }
void ensureDockNoActivate() { if (!visual) return; const auto handle = nativeHandle(visual); SetWindowLongPtrW(handle, GWL_EXSTYLE, GetWindowLongPtrW(handle, GWL_EXSTYLE) | WS_EX_NOACTIVATE); SetWindowPos(handle, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED); }
void positionDock();
bool isShellExperience(HWND window);
bool isDesktopWindow(HWND window);
void sendShortcut(WORD modifier, WORD key);
void sendQuickSettingsHotkey();

bool taskbarHasTrayButtons()
{
    const auto shell = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!shell || !IsWindowVisible(shell)) return false;
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IUIAutomation *uiAutomation = nullptr;
    IUIAutomationElement *root = nullptr;
    IUIAutomationCondition *condition = nullptr;
    IUIAutomationElementArray *elements = nullptr;
    int length = 0;
    if (SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&uiAutomation))) && uiAutomation && SUCCEEDED(uiAutomation->ElementFromHandle(shell, &root)) && root) {
        VARIANT value{};
        value.vt = VT_I4;
        value.lVal = UIA_ButtonControlTypeId;
        if (SUCCEEDED(uiAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, value, &condition)) && condition && SUCCEEDED(root->FindAll(TreeScope_Descendants, condition, &elements)) && elements) elements->get_Length(&length);
    }
    if (elements) elements->Release();
    if (condition) condition->Release();
    if (root) root->Release();
    if (uiAutomation) uiAutomation->Release();
    if (initialized == S_OK || initialized == S_FALSE) CoUninitialize();
    return length > 0;
}

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
    RECT bounds{};
    GetWindowRect(window, &bounds);
    const auto width = bounds.right - bounds.left;
    const auto height = bounds.bottom - bounds.top;
    const auto titledCore = _wcsicmp(title, L"Quick Settings") == 0 || _wcsicmp(title, L"Control Center") == 0 || _wcsicmp(title, L"Network") == 0 || _wcsicmp(title, L"Volume") == 0;
    const auto untitledCore = title[0] == L'\0' && width >= 200 && width <= 1000 && height >= 150 && height <= 1000;
    const auto core = isShellExperience(window) && wcscmp(className, L"Windows.UI.Core.CoreWindow") == 0 && (titledCore || untitledCore);
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
        QObject::connect(trayFlyoutTimer, &QTimer::timeout, [] { if (positionTrayFlyout(trayFlyoutAnchor)) trayFlyoutMisses = 0; else if (++trayFlyoutMisses > 60) trayFlyoutTimer->stop(); });
    }
    trayFlyoutAnchor = anchor;
    trayFlyoutMisses = 0;
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
    if (SendInput(4, input, sizeof(INPUT)) == 4) return;
    keybd_event(static_cast<BYTE>(modifier), 0, 0, 0);
    keybd_event(static_cast<BYTE>(key), 0, 0, 0);
    keybd_event(static_cast<BYTE>(key), 0, KEYEVENTF_KEYUP, 0);
    keybd_event(static_cast<BYTE>(modifier), 0, KEYEVENTF_KEYUP, 0);
}

void sendQuickSettingsHotkey() { sendShortcut(VK_LWIN, 'A'); }

void openQuickSettings()
{
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", L"ms-actioncenter:controlcenter/true", nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32) sendQuickSettingsHotkey();
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
    MONITORINFO monitor{ sizeof(monitor) };
    if (!visual || !GetMonitorInfoW(MonitorFromWindow(nativeHandle(visual), MONITOR_DEFAULTTONEAREST), &monitor)) return false;
    const auto &area = monitor.rcWork;
    return QProcess::startDetached(QCoreApplication::applicationFilePath(), { "--tasked-taskbar-guard", QString::number(GetCurrentProcessId()), QString::number(area.left), QString::number(area.top), QString::number(area.right), QString::number(area.bottom) });
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
        if (appbarRegistered && msg->hwnd == appbarHandle) {
            if (msg->message == WM_ACTIVATE) { APPBARDATA data{}; data.cbSize = sizeof(data); data.hWnd = appbarHandle; data.lParam = LOWORD(msg->wParam) != WA_INACTIVE; SHAppBarMessage(ABM_ACTIVATE, &data); }
            else if (msg->message == WM_WINDOWPOSCHANGED && !positioningDock) { APPBARDATA data{}; data.cbSize = sizeof(data); data.hWnd = appbarHandle; SHAppBarMessage(ABM_WINDOWPOSCHANGED, &data); }
            else if (msg->message == callbackMessage && msg->wParam == ABN_POSCHANGED) positionDock();
        }
        return false;
    }
};

void positionDock()
{
    if (!appbarRegistered || !visual || positioningDock) return;
    positioningDock = true;
    APPBARDATA data{};
    data.cbSize = sizeof(data);
    data.hWnd = appbarHandle;
    data.uEdge = dockPosition == 0 ? ABE_BOTTOM : dockPosition == 1 ? ABE_TOP : dockPosition == 2 ? ABE_LEFT : ABE_RIGHT;
    const auto monitorHandle = MonitorFromWindow(data.hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor{ sizeof(monitor) };
    GetMonitorInfoW(monitorHandle, &monitor);
    auto originalWorkArea = originalWorkAreas.find(monitorHandle);
    if (originalWorkArea == originalWorkAreas.end()) originalWorkArea = originalWorkAreas.emplace(monitorHandle, monitor.rcWork).first;
    const auto &workArea = takeover ? monitor.rcMonitor : originalWorkArea->second;
    const auto reservedEdge = dockPosition == 0 ? monitor.rcMonitor.bottom - workArea.bottom : dockPosition == 1 ? workArea.top - monitor.rcMonitor.top : dockPosition == 2 ? workArea.left - monitor.rcMonitor.left : monitor.rcMonitor.right - workArea.right;
    const auto reservedByOthers = takeover ? 0 : static_cast<int>((std::max)(0L, reservedEdge));
    const auto additionalThickness = (std::max)(1, dockHeight - reservedByOthers);
    data.rc = monitor.rcMonitor;
    if (dockPosition == 0) { data.rc.bottom = workArea.bottom; data.rc.top = data.rc.bottom - additionalThickness; }
    else if (dockPosition == 1) { data.rc.top = workArea.top; data.rc.bottom = data.rc.top + additionalThickness; }
    else if (dockPosition == 2) { data.rc.left = workArea.left; data.rc.right = data.rc.left + additionalThickness; }
    else { data.rc.right = workArea.right; data.rc.left = data.rc.right - additionalThickness; }
    const auto pinToMonitorEdge = [&] {
        if (dockPosition == 0) { data.rc.bottom = monitor.rcMonitor.bottom; data.rc.top = data.rc.bottom - additionalThickness; }
        else if (dockPosition == 1) { data.rc.top = monitor.rcMonitor.top; data.rc.bottom = data.rc.top + additionalThickness; }
        else if (dockPosition == 2) { data.rc.left = monitor.rcMonitor.left; data.rc.right = data.rc.left + additionalThickness; }
        else { data.rc.right = monitor.rcMonitor.right; data.rc.left = data.rc.right - additionalThickness; }
    };
    SHAppBarMessage(ABM_QUERYPOS, &data);
    if (takeover) pinToMonitorEdge();
    else if (dockPosition == 0) data.rc.top = data.rc.bottom - additionalThickness;
    else if (dockPosition == 1) data.rc.bottom = data.rc.top + additionalThickness;
    else if (dockPosition == 2) data.rc.right = data.rc.left + additionalThickness;
    else data.rc.left = data.rc.right - additionalThickness;
    RECT currentRect{};
    auto samePosition = GetWindowRect(data.hWnd, &currentRect) && currentRect.left == data.rc.left && currentRect.top == data.rc.top && currentRect.right == data.rc.right && currentRect.bottom == data.rc.bottom;
    if (!samePosition) SetWindowPos(data.hWnd, nullptr, data.rc.left, data.rc.top, data.rc.right - data.rc.left, data.rc.bottom - data.rc.top, SWP_NOACTIVATE | SWP_NOZORDER);
    SHAppBarMessage(ABM_SETPOS, &data);
    if (takeover) pinToMonitorEdge();
    samePosition = GetWindowRect(data.hWnd, &currentRect) && currentRect.left == data.rc.left && currentRect.top == data.rc.top && currentRect.right == data.rc.right && currentRect.bottom == data.rc.bottom;
    if (!samePosition) SetWindowPos(data.hWnd, nullptr, data.rc.left, data.rc.top, data.rc.right - data.rc.left, data.rc.bottom - data.rc.top, SWP_NOACTIVATE | SWP_NOZORDER);
    if (takeover) {
        auto workArea = originalWorkArea->second;
        if (dockPosition == 0) workArea.bottom = (std::min)(workArea.bottom, data.rc.top);
        else if (dockPosition == 1) workArea.top = (std::max)(workArea.top, data.rc.bottom);
        else if (dockPosition == 2) workArea.left = (std::max)(workArea.left, data.rc.right);
        else workArea.right = (std::min)(workArea.right, data.rc.left);
        MONITORINFO currentMonitor{ sizeof(currentMonitor) };
        if (workArea.right > workArea.left && workArea.bottom > workArea.top && GetMonitorInfoW(monitorHandle, &currentMonitor) && !EqualRect(&currentMonitor.rcWork, &workArea) && SystemParametersInfoW(SPI_SETWORKAREA, 0, &workArea, 0)) refitMaximizedWindows(monitorHandle, workArea);
    }
    const auto horizontal = dockPosition < 2;
    const auto screen = visual->screen() ? visual->screen() : QGuiApplication::primaryScreen();
    const auto screenGeometry = screen->geometry();
    const auto scale = visual->devicePixelRatio();
    const auto maximumLength = (std::max)(320, (horizontal ? screenGeometry.width() : screenGeometry.height()) - 32);
    const auto currentLength = horizontal ? visual->width() : visual->height();
    const auto length = (std::min)(maximumLength, (std::max)(320, currentLength));
    if (horizontal) {
        const auto edgeStart = dockPosition == 0 ? data.rc.top : monitor.rcMonitor.top;
        const auto y = screenGeometry.top() + qRound(static_cast<qreal>(edgeStart - monitor.rcMonitor.top) / scale);
        const auto height = (std::max)(1, qRound(static_cast<qreal>(dockHeight) / scale));
        visual->setGeometry(screenGeometry.left() + (screenGeometry.width() - length) / 2, y, length, height);
    } else {
        const auto edgeStart = dockPosition == 2 ? monitor.rcMonitor.left : data.rc.left;
        const auto x = screenGeometry.left() + qRound(static_cast<qreal>(edgeStart - monitor.rcMonitor.left) / scale);
        const auto width = (std::max)(1, qRound(static_cast<qreal>(dockHeight) / scale));
        visual->setGeometry(x, screenGeometry.top() + (screenGeometry.height() - length) / 2, width, length);
    }
    positioningDock = false;
}
}

void tasked::platform::prepareTaskbarSnapshot()
{
    restoreAllTaskbars();
    for (int attempt = 0; attempt < 30 && !taskbarHasTrayButtons(); ++attempt) Sleep(50);
}
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
    ensureDockNoActivate();
    dockHeight = qRound(height * visualWindow->devicePixelRatio());
    dockPosition = (std::max)(0, (std::min)(3, position));
    taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
    const auto taskbarGuardStarted = startTaskbarGuard();
    if (taskbarGuardStarted) { restoreAllTaskbars(); QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents); }
    reservation = new QWindow;
    reservation->setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowTransparentForInput);
    reservation->setOpacity(0.0);
    reservation->show();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    APPBARDATA data{};
    data.cbSize = sizeof(data);
    data.hWnd = nativeHandle(reservation);
    data.uCallbackMessage = callbackMessage;
    appbarHandle = data.hWnd;
    appbarRegistered = SHAppBarMessage(ABM_NEW, &data) != 0;
    eventFilter = new DockEventFilter;
    qApp->installNativeEventFilter(eventFilter);
    QObject::connect(visual, &QWindow::widthChanged, visual, [](int width) { if (dockPosition > 1) dockHeight = qRound(width * visual->devicePixelRatio()); positionDock(); });
    QObject::connect(visual, &QWindow::heightChanged, visual, [](int height) { if (dockPosition < 2) dockHeight = qRound(height * visual->devicePixelRatio()); positionDock(); });
    QObject::connect(visual, &QWindow::visibleChanged, visual, [](bool visible) { if (visible) ensureDockNoActivate(); });
    QObject::connect(visual, &QWindow::activeChanged, visual, [] { if (appbarRegistered) { APPBARDATA data{}; data.cbSize = sizeof(data); data.hWnd = appbarHandle; data.lParam = visual->isActive(); SHAppBarMessage(ABM_ACTIVATE, &data); } });
    fullscreenTimer = new QTimer(qApp);
    fullscreenTimer->setInterval(250);
    QObject::connect(fullscreenTimer, &QTimer::timeout, updateFullscreenVisibility);
    fullscreenTimer->start();
    positionDock();
    QTimer::singleShot(0, visual, [] { ensureDockNoActivate(); });
    if (taskbarGuardStarted) { hideTaskbars(); takeover = true; positionDock(); QTimer::singleShot(500, [] { positionDock(); }); }
    return visual->geometry();
}

void tasked::platform::setDockPosition(int position)
{
    dockPosition = (std::max)(0, (std::min)(3, position));
    if (visual) dockHeight = qRound((dockPosition < 2 ? visual->height() : visual->width()) * visual->devicePixelRatio());
    positionDock();
}

void tasked::platform::uninstallDock()
{
    if (appbarRegistered) {
        APPBARDATA data{};
        data.cbSize = sizeof(data);
        data.hWnd = appbarHandle;
        SHAppBarMessage(ABM_REMOVE, &data);
        appbarRegistered = false;
        appbarHandle = nullptr;
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
        for (const auto &[monitor, originalWorkArea] : originalWorkAreas) { auto workArea = originalWorkArea; if (SystemParametersInfoW(SPI_SETWORKAREA, 0, &workArea, 0)) refitMaximizedWindows(monitor, workArea); }
        takeover = false;
    }
    originalWorkAreas.clear();
    delete reservation;
    reservation = nullptr;
    visual = nullptr;
}

int tasked::platform::runTaskbarGuard(quint32 parentPid, const QRect &originalWorkArea)
{
    const auto parent = OpenProcess(SYNCHRONIZE, FALSE, parentPid);
    if (parent) {
        WaitForSingleObject(parent, INFINITE);
        CloseHandle(parent);
    }
    restoreAllTaskbars();
    RECT area{ originalWorkArea.x(), originalWorkArea.y(), originalWorkArea.x() + originalWorkArea.width(), originalWorkArea.y() + originalWorkArea.height() };
    SystemParametersInfoW(SPI_SETWORKAREA, 0, &area, 0);
    return 0;
}
