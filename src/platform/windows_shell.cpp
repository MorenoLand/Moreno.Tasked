#include "windows_shell.h"

#include <QFileInfo>
#include <QImage>
#include <QRect>
#include <QReadLocker>
#include <QWriteLocker>
#include <QSet>
#include <QSettings>
#include <QThread>
#include <QUrl>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <UIAutomation.h>
#include <oleauto.h>
#endif

#include <algorithm>
#include <array>
#include <utility>

namespace {
QHash<int, QByteArray> windowRoleNames() { return {{RunningAppsModel::TitleRole, "title"}, {RunningAppsModel::WindowRole, "windowHandle"}, {RunningAppsModel::IconSourceRole, "iconSource"}, {RunningAppsModel::ActiveRole, "active"}}; }
QHash<int, QByteArray> trayRoleNames() { return {{TrayModel::KeyRole, "key"}, {TrayModel::TooltipRole, "tooltip"}}; }

#ifdef Q_OS_WIN
HWND traySpyWindow = nullptr;
bool traySpyClassOwned = false;
QVector<TrayModel::Item> observedTrayItems;

#pragma pack(push, 4)
struct ShellNotifyIconDataPrefix { quint32 callbackSize; quint32 owner; quint32 id; quint32 flags; quint32 callback; quint32 icon; wchar_t tooltip[128]; quint32 state; quint32 stateMask; wchar_t info[256]; quint32 version; };
struct ShellTrayMessage { qint32 magic; quint32 messageType; ShellNotifyIconDataPrefix iconData; };
#pragma pack(pop)

LRESULT CALLBACK traySpyWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

QString processPath(HWND window)
{
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    const auto process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (!process) return {};
    std::array<wchar_t, 32768> path{};
    DWORD length = static_cast<DWORD>(path.size());
    const auto ok = QueryFullProcessImageNameW(process, 0, path.data(), &length);
    CloseHandle(process);
    return ok ? QString::fromWCharArray(path.data(), static_cast<int>(length)) : QString();
}

BOOL CALLBACK findRealTrayWindow(HWND window, LPARAM parameter)
{
    wchar_t className[64]{};
    GetClassNameW(window, className, ARRAYSIZE(className));
    if (window != traySpyWindow && wcscmp(className, L"Shell_TrayWnd") == 0) {
        *reinterpret_cast<HWND *>(parameter) = window;
        return FALSE;
    }
    return TRUE;
}

HWND realTrayWindow()
{
    HWND result = nullptr;
    EnumWindows(findRealTrayWindow, reinterpret_cast<LPARAM>(&result));
    return result;
}

qulonglong observedTrayKey(qulonglong owner, quint32 id) { return owner ^ (static_cast<qulonglong>(id) << 32); }

void observeTrayMessage(const COPYDATASTRUCT *copy)
{
    if (!copy || copy->dwData != 1 || !copy->lpData) return;
    const auto *message = static_cast<const ShellTrayMessage *>(copy->lpData);
    const auto owner = static_cast<qulonglong>(message->iconData.owner);
    const auto id = message->iconData.id;
    if (!owner) return;
    const auto key = observedTrayKey(owner, id);
    const auto found = std::find_if(observedTrayItems.begin(), observedTrayItems.end(), [owner, id](const TrayModel::Item &item) { return item.owner == owner && item.id == id; });
    if (message->messageType == NIM_DELETE) {
        if (found != observedTrayItems.end()) observedTrayItems.erase(found);
        return;
    }
    auto update = found;
    if (update == observedTrayItems.end()) {
        TrayModel::Item item;
        item.key = key;
        item.owner = owner;
        item.id = id;
        observedTrayItems.append(item);
        update = observedTrayItems.end() - 1;
    }
    const auto flags = message->iconData.flags;
    update->owner = owner;
    update->id = id;
    if (flags & NIF_MESSAGE) update->callback = message->iconData.callback;
    if (flags & NIF_ICON) update->icon = message->iconData.icon;
    if (flags & NIF_TIP) {
        const auto length = std::find(std::begin(message->iconData.tooltip), std::end(message->iconData.tooltip), wchar_t{}) - std::begin(message->iconData.tooltip);
        update->tooltip = QString::fromWCharArray(message->iconData.tooltip, static_cast<int>(length));
    }
    if (message->iconData.version <= 4) update->version = message->iconData.version;
}

bool trayTextMatches(const QString &first, const QString &second)
{
    return !first.isEmpty() && !second.isEmpty() && (first.compare(second, Qt::CaseInsensitive) == 0 || first.contains(second, Qt::CaseInsensitive) || second.contains(first, Qt::CaseInsensitive));
}

void applyObservedTrayItems(QVector<TrayModel::Item> &next)
{
    QSet<qulonglong> used;
    for (auto &item : next) {
        auto found = std::find_if(observedTrayItems.cbegin(), observedTrayItems.cend(), [&item, &used](const TrayModel::Item &candidate) { return candidate.owner && !used.contains(candidate.key) && item.owner == candidate.owner && item.id == candidate.id; });
        if (found == observedTrayItems.cend()) found = std::find_if(observedTrayItems.cbegin(), observedTrayItems.cend(), [&item, &used](const TrayModel::Item &candidate) { return candidate.owner && !used.contains(candidate.key) && trayTextMatches(item.tooltip, candidate.tooltip); });
        if (found == observedTrayItems.cend()) continue;
        used.insert(found->key);
        item.owner = found->owner;
        item.id = found->id;
        item.callback = found->callback;
        item.version = found->version;
        if (found->icon) item.icon = found->icon;
    }
}

LRESULT CALLBACK traySpyWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_COPYDATA) {
        observeTrayMessage(reinterpret_cast<const COPYDATASTRUCT *>(lParam));
        const auto real = realTrayWindow();
        if (real) return SendMessageW(real, message, wParam, lParam);
    }
    if (message == WM_TIMER) {
        SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void installTraySpy()
{
    if (traySpyWindow) return;
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW windowClass{ sizeof(WNDCLASSEXW), 0, traySpyWindowProc, 0, 0, instance, nullptr, nullptr, nullptr, nullptr, L"Shell_TrayWnd", nullptr };
    if (!RegisterClassExW(&windowClass)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
    } else traySpyClassOwned = true;
    traySpyWindow = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"Shell_TrayWnd", nullptr, WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (!traySpyWindow) {
        if (traySpyClassOwned) UnregisterClassW(L"Shell_TrayWnd", instance);
        traySpyClassOwned = false;
        return;
    }
    SetWindowPos(traySpyWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetTimer(traySpyWindow, 1, 100, nullptr);
    const auto taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    if (taskbarCreated) SendNotifyMessageW(HWND_BROADCAST, taskbarCreated, 0, 0);
}

void uninstallTraySpy()
{
    if (traySpyWindow) {
        KillTimer(traySpyWindow, 1);
        DestroyWindow(traySpyWindow);
        traySpyWindow = nullptr;
    }
    if (traySpyClassOwned) {
        UnregisterClassW(L"Shell_TrayWnd", GetModuleHandleW(nullptr));
        traySpyClassOwned = false;
    }
    observedTrayItems.clear();
}

QPixmap iconFromPath(const QString &path, const QSize &requested)
{
    if (path.isEmpty()) return {};
    SHFILEINFOW fileInfo{};
    if (!SHGetFileInfoW(reinterpret_cast<LPCWSTR>(path.utf16()), 0, &fileInfo, sizeof(fileInfo), SHGFI_ICON | SHGFI_LARGEICON) || !fileInfo.hIcon) return {};
    const auto image = QImage::fromHICON(fileInfo.hIcon);
    DestroyIcon(fileInfo.hIcon);
    return QPixmap::fromImage(image.scaled(requested, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QImage captureTrayIcon(const RECT &bounds)
{
    const auto width = bounds.right - bounds.left;
    const auto height = bounds.bottom - bounds.top;
    const auto size = (std::min)(32L, (std::min)(width, height));
    if (size <= 0) return {};
    const auto screen = GetDC(nullptr);
    const auto memory = CreateCompatibleDC(screen);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = size;
    info.bmiHeader.biHeight = -size;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    const auto bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!screen || !memory || !bitmap || !bits) {
        if (bitmap) DeleteObject(bitmap);
        if (memory) DeleteDC(memory);
        if (screen) ReleaseDC(nullptr, screen);
        return {};
    }
    const auto previous = SelectObject(memory, bitmap);
    const auto x = bounds.left + (width - size) / 2;
    const auto y = bounds.top + (height - size) / 2;
    const auto copied = BitBlt(memory, 0, 0, size, size, screen, x, y, SRCCOPY | CAPTUREBLT);
    QImage image;
    if (copied) image = QImage(static_cast<uchar *>(bits), size, size, QImage::Format_ARGB32_Premultiplied).copy();
    SelectObject(memory, previous);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(nullptr, screen);
    if (image.isNull()) return {};
    image = image.convertToFormat(QImage::Format_ARGB32);
    const std::array<QRgb, 4> backgrounds{ image.pixel(0, 0), image.pixel(size - 1, 0), image.pixel(0, size - 1), image.pixel(size - 1, size - 1) };
    const auto blend = [](QRgb first, QRgb second, int step, int total) { return qRgba(qRed(first) + (qRed(second) - qRed(first)) * step / total, qGreen(first) + (qGreen(second) - qGreen(first)) * step / total, qBlue(first) + (qBlue(second) - qBlue(first)) * step / total, 255); };
    for (int yPixel = 0; yPixel < image.height(); ++yPixel) for (int xPixel = 0; xPixel < image.width(); ++xPixel) {
        const auto pixel = image.pixel(xPixel, yPixel);
        const auto top = blend(backgrounds.at(0), backgrounds.at(1), xPixel, size - 1);
        const auto bottom = blend(backgrounds.at(2), backgrounds.at(3), xPixel, size - 1);
        const auto background = blend(top, bottom, yPixel, size - 1);
        const auto distance = qAbs(qRed(pixel) - qRed(background)) + qAbs(qGreen(pixel) - qGreen(background)) + qAbs(qBlue(pixel) - qBlue(background));
        const auto alpha = qBound(0, (distance - 48) * 8, 255);
        image.setPixel(xPixel, yPixel, qRgba(qRed(pixel), qGreen(pixel), qBlue(pixel), alpha));
    }
    QRect content;
    for (int yPixel = 0; yPixel < image.height(); ++yPixel) for (int xPixel = 0; xPixel < image.width(); ++xPixel) if (qAlpha(image.pixel(xPixel, yPixel)) > 40) content |= QRect(xPixel, yPixel, 1, 1);
    return content.isValid() ? image.copy(content.adjusted(-2, -2, 2, 2).intersected(image.rect())) : QImage();
}

QImage trayIconImage(quintptr handle)
{
    const auto copy = CopyIcon(reinterpret_cast<HICON>(handle));
    if (!copy) return {};
    QImage image;
    ICONINFO iconInfo{};
    if (GetIconInfo(copy, &iconInfo)) {
        BITMAP bitmap{};
        if (iconInfo.hbmColor && GetObjectW(iconInfo.hbmColor, sizeof(bitmap), &bitmap) == sizeof(bitmap) && bitmap.bmWidth > 0 && bitmap.bmHeight > 0) {
            const auto width = bitmap.bmWidth;
            const auto height = bitmap.bmHeight;
            image = QImage(width, height, QImage::Format_ARGB32);
            BITMAPINFO bitmapInfo{};
            bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bitmapInfo.bmiHeader.biWidth = width;
            bitmapInfo.bmiHeader.biHeight = -height;
            bitmapInfo.bmiHeader.biPlanes = 1;
            bitmapInfo.bmiHeader.biBitCount = 32;
            bitmapInfo.bmiHeader.biCompression = BI_RGB;
            const auto dc = GetDC(nullptr);
            const auto colorRows = dc && GetDIBits(dc, iconInfo.hbmColor, 0, height, image.bits(), &bitmapInfo, DIB_RGB_COLORS) == height;
            if (colorRows && iconInfo.hbmMask) {
                bool hasAlpha = false;
                for (int y = 0; y < height && !hasAlpha; ++y) for (int x = 0; x < width && !hasAlpha; ++x) hasAlpha = qAlpha(image.pixel(x, y)) != 0;
                if (!hasAlpha) {
                    QImage mask(width, height, QImage::Format_ARGB32);
                    if (GetDIBits(dc, iconInfo.hbmMask, 0, height, mask.bits(), &bitmapInfo, DIB_RGB_COLORS) == height) for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) image.setPixel(x, y, (image.pixel(x, y) & 0x00ffffff) | (qRed(mask.pixel(x, y)) == 255 ? 0 : 0xff000000));
                }
            } else if (!colorRows) image = {};
            if (dc) ReleaseDC(nullptr, dc);
        }
        if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
        if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
    }
    if (image.isNull()) image = QImage::fromHICON(copy);
    DestroyIcon(copy);
    return image;
}

IUIAutomation *automation()
{
    static IUIAutomation *instance = nullptr;
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&instance));
    }
    return instance;
}

QString automationString(BSTR value)
{
    const auto result = value ? QString::fromWCharArray(value) : QString();
    if (value) SysFreeString(value);
    return result;
}

qulonglong automationKey(const QString &automationId, const QString &name, int ordinal) { return static_cast<qulonglong>(qHash(automationId + QLatin1Char('\n') + name + QLatin1Char('\n') + QString::number(ordinal))); }

QVector<TrayModel::Item> automationTrayItems(bool capture)
{
    QVector<TrayModel::Item> result;
    auto *uiAutomation = automation();
    const auto shell = realTrayWindow();
    if (!uiAutomation || !shell) return result;
    IUIAutomationElement *root = nullptr;
    if (FAILED(uiAutomation->ElementFromHandle(shell, &root))) return result;
    VARIANT value{};
    value.vt = VT_I4;
    value.lVal = UIA_ButtonControlTypeId;
    IUIAutomationCondition *condition = nullptr;
    IUIAutomationElementArray *elements = nullptr;
    uiAutomation->CreatePropertyCondition(UIA_ControlTypePropertyId, value, &condition);
    if (!condition || FAILED(root->FindAll(TreeScope_Descendants, condition, &elements))) {
        if (condition) condition->Release();
        root->Release();
        return result;
    }
    QHash<QString, int> ordinals;
    int length = 0;
    elements->get_Length(&length);
    for (int index = 0; index < length; ++index) {
        IUIAutomationElement *element = nullptr;
        if (FAILED(elements->GetElement(index, &element)) || !element) continue;
        const auto name = automationString([&] { BSTR value = nullptr; element->get_CurrentName(&value); return value; }());
        const auto className = automationString([&] { BSTR value = nullptr; element->get_CurrentClassName(&value); return value; }());
        const auto automationId = automationString([&] { BSTR value = nullptr; element->get_CurrentAutomationId(&value); return value; }());
        if (!className.startsWith(QStringLiteral("SystemTray.")) || className == QStringLiteral("SystemTray.ShowDesktopButton") || name.startsWith(QStringLiteral("Clock "))) {
            element->Release();
            continue;
        }
        const auto descriptor = automationId + QLatin1Char('\n') + name;
        const auto ordinal = ordinals[descriptor]++;
        RECT bounds{};
        BOOL offscreen = FALSE;
        element->get_CurrentBoundingRectangle(&bounds);
        element->get_CurrentIsOffscreen(&offscreen);
        TrayModel::Item item;
        item.key = automationKey(automationId, name, ordinal);
        item.tooltip = name.isEmpty() ? className : name;
        item.automationId = automationId;
        item.ordinal = ordinal;
        item.automationElement = reinterpret_cast<quintptr>(element);
        if (bounds.right > bounds.left && bounds.bottom > bounds.top) item.bounds = QRect(bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top);
        if (capture && !offscreen && IsWindowVisible(shell)) item.image = captureTrayIcon(bounds);
        result.append(item);
    }
    elements->Release();
    condition->Release();
    root->Release();
    return result;
}

void releaseAutomationItems(QVector<TrayModel::Item> &items)
{
    for (auto &item : items) if (item.automationElement) reinterpret_cast<IUIAutomationElement *>(item.automationElement)->Release();
}

bool invokeAutomationItem(quintptr elementPointer)
{
    if (!elementPointer) return false;
    auto *element = reinterpret_cast<IUIAutomationElement *>(elementPointer);
    IUIAutomationInvokePattern *pattern = nullptr;
    if (FAILED(element->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&pattern))) || !pattern) return false;
    const auto result = SUCCEEDED(pattern->Invoke());
    pattern->Release();
    return result;
}

struct WindowRefreshContext { QVector<RunningAppsModel::Item> items; HWND foreground = nullptr; };

BOOL CALLBACK enumerateWindows(HWND window, LPARAM parameter)
{
    auto &context = *reinterpret_cast<WindowRefreshContext *>(parameter);
    if (!IsWindowVisible(window) || GetWindow(window, GW_OWNER)) return TRUE;
    if (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked) return TRUE;
    std::array<wchar_t, 512> title{};
    const auto length = GetWindowTextW(window, title.data(), static_cast<int>(title.size()));
    if (length <= 0) return TRUE;
    const auto path = processPath(window);
    context.items.append({ static_cast<qulonglong>(reinterpret_cast<quintptr>(window)), QString::fromWCharArray(title.data(), length), QStringLiteral("image://shell/window/") + QString::number(static_cast<qulonglong>(reinterpret_cast<quintptr>(window))), window == context.foreground });
    return TRUE;
}

BOOL CALLBACK collectToolbars(HWND window, LPARAM parameter)
{
    auto &bars = *reinterpret_cast<QVector<HWND> *>(parameter);
    wchar_t className[64]{};
    GetClassNameW(window, className, ARRAYSIZE(className));
    if (wcscmp(className, L"ToolbarWindow32") == 0) bars.append(window);
    return TRUE;
}

void addToolbars(HWND root, QVector<HWND> &bars)
{
    if (!root) return;
    QVector<HWND> found;
    EnumChildWindows(root, collectToolbars, reinterpret_cast<LPARAM>(&found));
    for (const auto bar : found) if (!bars.contains(bar)) bars.append(bar);
}

BOOL CALLBACK collectTaskbarToolbars(HWND window, LPARAM parameter)
{
    wchar_t className[64]{};
    GetClassNameW(window, className, ARRAYSIZE(className));
    if (wcscmp(className, L"Shell_TrayWnd") == 0 || wcscmp(className, L"Shell_SecondaryTrayWnd") == 0) addToolbars(window, *reinterpret_cast<QVector<HWND> *>(parameter));
    return TRUE;
}

QVector<HWND> trayToolbars()
{
    QVector<HWND> bars;
    EnumWindows(collectTaskbarToolbars, reinterpret_cast<LPARAM>(&bars));
    addToolbars(FindWindowW(L"NotifyIconOverflowWindow", nullptr), bars);
    return bars;
}

struct RemoteTrayData { HWND owner; UINT id; UINT callback; UINT state; UINT version; HICON icon; };

void enumerateToolbar(HWND toolbar, QVector<TrayModel::Item> &items)
{
    DWORD processId = 0;
    GetWindowThreadProcessId(toolbar, &processId);
    const auto process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, processId);
    if (!process) return;
    const auto remote = VirtualAllocEx(process, nullptr, sizeof(TBBUTTON), MEM_COMMIT, PAGE_READWRITE);
    if (!remote) {
        CloseHandle(process);
        return;
    }
    const auto remoteRect = VirtualAllocEx(process, nullptr, sizeof(RECT), MEM_COMMIT, PAGE_READWRITE);
    const auto count = static_cast<int>(SendMessageW(toolbar, TB_BUTTONCOUNT, 0, 0));
    for (int index = 0; index < count; ++index) {
        TBBUTTON button{};
        if (!SendMessageW(toolbar, TB_GETBUTTON, index, reinterpret_cast<LPARAM>(remote)) || !ReadProcessMemory(process, remote, &button, sizeof(button), nullptr)) continue;
        if (!button.dwData || button.fsState & TBSTATE_HIDDEN || button.fsStyle & TBSTYLE_SEP) continue;
        RemoteTrayData tray{};
        if (!ReadProcessMemory(process, reinterpret_cast<const void *>(button.dwData), &tray, sizeof(tray), nullptr) || !tray.owner || !tray.callback) continue;
        const auto key = static_cast<qulonglong>(reinterpret_cast<quintptr>(tray.owner)) ^ (static_cast<qulonglong>(tray.id) << 32) ^ static_cast<qulonglong>(reinterpret_cast<quintptr>(toolbar));
        const auto path = processPath(tray.owner);
        TrayModel::Item item;
        item.key = key;
        item.owner = static_cast<qulonglong>(reinterpret_cast<quintptr>(tray.owner));
        item.id = tray.id;
        item.callback = tray.callback;
        item.version = tray.version;
        item.icon = reinterpret_cast<quintptr>(tray.icon);
        item.tooltip = path.isEmpty() ? QStringLiteral("Tray icon") : QFileInfo(path).fileName();
        if (remoteRect) {
            RECT bounds{};
            if (SendMessageW(toolbar, TB_GETITEMRECT, index, reinterpret_cast<LPARAM>(remoteRect)) && ReadProcessMemory(process, remoteRect, &bounds, sizeof(bounds), nullptr)) {
                MapWindowPoints(toolbar, nullptr, reinterpret_cast<POINT *>(&bounds), 2);
                item.bounds = QRect(bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top);
            }
        }
        items.append(item);
    }
    if (remoteRect) VirtualFreeEx(process, remoteRect, 0, MEM_RELEASE);
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    CloseHandle(process);
}

void enrichTrayItems(QVector<TrayModel::Item> &items, const QVector<TrayModel::Item> &native)
{
    QSet<int> used;
    for (auto &item : items) {
        const auto found = std::find_if(native.cbegin(), native.cend(), [&item, &used, &native](const TrayModel::Item &candidate) { const auto index = static_cast<int>(&candidate - native.constData()); return !used.contains(index) && item.bounds.isValid() && candidate.bounds.isValid() && (item.bounds.contains(candidate.bounds.center()) || candidate.bounds.contains(item.bounds.center())); });
        if (found == native.cend()) continue;
        const auto index = static_cast<int>(&(*found) - native.constData());
        used.insert(index);
        item.owner = found->owner;
        item.id = found->id;
        item.callback = found->callback;
        item.version = found->version;
        item.icon = found->icon;
    }
}
#endif
}

RunningAppsModel::RunningAppsModel(QObject *parent) : QAbstractListModel(parent)
{
    connect(&timer, &QTimer::timeout, this, &RunningAppsModel::refresh);
    timer.start(500);
    refresh();
}

int RunningAppsModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : items.size(); }

QVariant RunningAppsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= items.size()) return {};
    const auto &item = items.at(index.row());
    if (role == TitleRole) return item.title;
    if (role == WindowRole) return QString::number(item.window);
    if (role == IconSourceRole) return item.iconSource;
    if (role == ActiveRole) return item.active;
    return {};
}

QHash<int, QByteArray> RunningAppsModel::roleNames() const { return windowRoleNames(); }

void RunningAppsModel::refresh()
{
#ifdef Q_OS_WIN
    WindowRefreshContext context{ {}, GetForegroundWindow() };
    EnumWindows(enumerateWindows, reinterpret_cast<LPARAM>(&context));
    if (!items.isEmpty() && !context.items.isEmpty()) {
        QVector<Item> ordered;
        QSet<qulonglong> placed;
        for (const auto &old : items) {
            const auto found = std::find_if(context.items.begin(), context.items.end(), [&old](const Item &item) { return item.window == old.window; });
            if (found != context.items.end()) {
                placed.insert(found->window);
                ordered.append(std::move(*found));
            }
        }
        for (auto &item : context.items) if (!placed.contains(item.window)) ordered.append(std::move(item));
        context.items = std::move(ordered);
    }
    if (context.items == items) return;
    beginResetModel();
    items = std::move(context.items);
    endResetModel();
#endif
}

void RunningAppsModel::move(int from, int to)
{
    if (from < 0 || to < 0 || from >= items.size() || to >= items.size() || from == to) return;
    if (!beginMoveRows(QModelIndex(), from, from, QModelIndex(), to > from ? to + 1 : to)) return;
    items.move(from, to);
    endMoveRows();
}

void RunningAppsModel::activate(const QString &windowHandle)
{
#ifdef Q_OS_WIN
    bool ok = false;
    const auto value = windowHandle.toULongLong(&ok);
    const auto window = reinterpret_cast<HWND>(static_cast<quintptr>(value));
    if (ok && IsWindow(window)) {
        const auto foreground = GetForegroundWindow();
        const auto foregroundRoot = foreground ? GetAncestor(foreground, GA_ROOT) : nullptr;
        const auto targetRoot = GetAncestor(window, GA_ROOT);
        const auto dock = FindWindowW(nullptr, L"Tasked");
        const auto dockRoot = dock ? GetAncestor(dock, GA_ROOT) : nullptr;
        const auto cachedActive = std::any_of(items.cbegin(), items.cend(), [window](const Item &item) { return item.window == static_cast<qulonglong>(reinterpret_cast<quintptr>(window)) && item.active; });
        const auto foregroundTask = foreground == window || foregroundRoot == targetRoot;
        const auto dockWasForeground = foreground == dock || foregroundRoot == dockRoot;
        if (!IsIconic(window) && (foregroundTask || (dockWasForeground && cachedActive))) {
            ShowWindow(window, SW_MINIMIZE);
            return;
        }
        const auto targetThread = GetWindowThreadProcessId(window, nullptr);
        const auto currentThread = GetCurrentThreadId();
        DWORD targetProcess = 0;
        GetWindowThreadProcessId(window, &targetProcess);
        AllowSetForegroundWindow(targetProcess);
        const auto attached = targetThread && targetThread != currentThread && AttachThreadInput(currentThread, targetThread, TRUE);
        WINDOWPLACEMENT placement{ sizeof(WINDOWPLACEMENT) };
        if (GetWindowPlacement(window, &placement) && placement.showCmd == SW_SHOWMINIMIZED) {
            placement.showCmd = SW_RESTORE;
            SetWindowPlacement(window, &placement);
        }
        ShowWindow(window, SW_RESTORE);
        BringWindowToTop(window);
        SetForegroundWindow(window);
        if (GetForegroundWindow() != window) SwitchToThisWindow(window, TRUE);
        if (attached) SetFocus(window);
        if (attached) AttachThreadInput(currentThread, targetThread, FALSE);
    }
#else
    Q_UNUSED(windowHandle);
#endif
}

void RunningAppsModel::showTaskMenu(const QString &windowHandle, int x, int y)
{
#ifdef Q_OS_WIN
    bool ok = false;
    const auto value = windowHandle.toULongLong(&ok);
    const auto window = reinterpret_cast<HWND>(static_cast<quintptr>(value));
    if (!ok || !IsWindow(window)) return;
    const auto menu = GetSystemMenu(window, FALSE);
    if (!menu) return;
    const auto owner = FindWindowW(nullptr, L"Tasked");
    if (!owner) return;
    SetForegroundWindow(owner);
    const auto command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, x, y, owner, nullptr);
    PostMessageW(owner, WM_NULL, 0, 0);
    if (command) PostMessageW(window, WM_SYSCOMMAND, command, 0);
#else
    Q_UNUSED(windowHandle);
    Q_UNUSED(x);
    Q_UNUSED(y);
#endif
}

void RunningAppsModel::close(const QString &windowHandle)
{
#ifdef Q_OS_WIN
    bool ok = false;
    const auto value = windowHandle.toULongLong(&ok);
    const auto window = reinterpret_cast<HWND>(static_cast<quintptr>(value));
    if (ok && IsWindow(window)) PostMessageW(window, WM_CLOSE, 0, 0);
#else
    Q_UNUSED(windowHandle);
#endif
}

TrayModel::TrayModel(QObject *parent) : QAbstractListModel(parent)
{
#ifdef Q_OS_WIN
    installTraySpy();
#endif
    overflowKeys = QSettings().value("tray/overflowKeys").toStringList();
    trayOrder = QSettings().value("tray/order").toStringList();
    connect(&timer, &QTimer::timeout, this, &TrayModel::refresh);
    timer.start(1000);
    for (int attempt = 0; attempt < 10 && items.isEmpty(); ++attempt) {
        refresh();
        if (items.isEmpty()) QThread::msleep(100);
    }
}

bool TrayModel::isOverflow(const QString &key) const { return overflowKeys.contains(key); }
bool TrayModel::isSystemFlyoutItem(const QString &key) const
{
    bool ok = false;
    const auto value = key.toULongLong(&ok);
    const auto found = std::find_if(items.cbegin(), items.cend(), [value, ok](const Item &item) { return ok && item.key == value; });
    if (found == items.cend()) return false;
    if (found->automationId != QStringLiteral("SystemTrayIcon")) return false;
    const auto text = (found->tooltip + QLatin1Char(' ') + found->automationId).toLower();
    return text.contains(QStringLiteral("volume")) || text.contains(QStringLiteral("sound")) || text.contains(QStringLiteral("speaker")) || text.contains(QStringLiteral("audio")) || text.contains(QStringLiteral("network")) || text.contains(QStringLiteral("wi-fi")) || text.contains(QStringLiteral("wifi")) || text.contains(QStringLiteral("internet")) || text.contains(QStringLiteral("bluetooth")) || text.contains(QStringLiteral("battery"));
}
void TrayModel::setOverflow(const QString &key, bool enabled)
{
    if (key.isEmpty() || isOverflow(key) == enabled) return;
    if (enabled) overflowKeys.append(key); else overflowKeys.removeAll(key);
    QSettings().setValue("tray/overflowKeys", overflowKeys);
    emit overflowChanged();
}

void TrayModel::applySavedOrder(QVector<Item> &next) const
{
    if (trayOrder.isEmpty()) return;
    QVector<Item> ordered;
    QSet<qulonglong> placed;
    for (const auto &key : trayOrder) {
        bool ok = false;
        const auto value = key.toULongLong(&ok);
        const auto found = std::find_if(next.cbegin(), next.cend(), [value, ok](const Item &item) { return ok && item.key == value; });
        if (found != next.cend()) {
            placed.insert(found->key);
            ordered.append(*found);
        }
    }
    for (const auto &item : next) if (!placed.contains(item.key)) ordered.append(item);
    next = std::move(ordered);
}

void TrayModel::saveOrder()
{
    trayOrder.clear();
    for (const auto &item : items) trayOrder.append(QString::number(item.key));
    QSettings().setValue("tray/order", trayOrder);
}

void TrayModel::reorder(const QStringList &orderedKeys, bool overflowOnly)
{
    QVector<Item> reordered;
    QSet<qulonglong> placed;
    for (const auto &key : orderedKeys) {
        bool ok = false;
        const auto value = key.toULongLong(&ok);
        const auto found = std::find_if(items.cbegin(), items.cend(), [value, ok, this, overflowOnly](const Item &item) { return ok && item.key == value && isOverflow(QString::number(item.key)) == overflowOnly; });
        if (found != items.cend()) {
            placed.insert(found->key);
            reordered.append(*found);
        }
    }
    for (const auto &item : items) if (isOverflow(QString::number(item.key)) == overflowOnly && !placed.contains(item.key)) {
        placed.insert(item.key);
        reordered.append(item);
    }
    for (const auto &item : items) if (!placed.contains(item.key)) reordered.append(item);
    if (reordered.size() != items.size()) return;
    bool changed = false;
    for (int index = 0; index < items.size(); ++index) if (items.at(index).key != reordered.at(index).key) { changed = true; break; }
    if (!changed) return;
    beginResetModel();
    items = std::move(reordered);
    endResetModel();
    saveOrder();
}

TrayModel::~TrayModel()
{
#ifdef Q_OS_WIN
    uninstallTraySpy();
    releaseAutomationItems(items);
#endif
}

int TrayModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : items.size(); }

QVariant TrayModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= items.size()) return {};
    const auto &item = items.at(index.row());
    if (role == KeyRole) return QString::number(item.key);
    if (role == TooltipRole) return item.tooltip;
    return {};
}

QHash<int, QByteArray> TrayModel::roleNames() const { return trayRoleNames(); }

void TrayModel::refresh()
{
#ifdef Q_OS_WIN
    const auto shell = realTrayWindow();
    const auto recover = shell && !IsWindowVisible(shell) && items.isEmpty();
    if (recover) {
        ShowWindow(shell, SW_SHOWNOACTIVATE);
        Sleep(250);
    }
    const auto visible = shell && IsWindowVisible(shell);
    QVector<Item> next = automationTrayItems(visible);
    QVector<Item> native;
    for (const auto toolbar : trayToolbars()) enumerateToolbar(toolbar, native);
    if (next.isEmpty()) {
        next = std::move(native);
        if (next.isEmpty() && !visible) {
            if (recover) ShowWindow(shell, SW_HIDE);
            return;
        }
    } else enrichTrayItems(next, native);
    applyObservedTrayItems(next);
    if (!items.isEmpty() && !next.isEmpty()) {
        QVector<Item> ordered;
        QSet<qulonglong> placed;
        for (const auto &old : items) {
            const auto found = std::find_if(next.begin(), next.end(), [&old](const Item &item) { return item.key == old.key; });
            if (found != next.end()) {
                placed.insert(found->key);
                ordered.append(std::move(*found));
            }
        }
        for (auto &item : next) if (!placed.contains(item.key)) ordered.append(std::move(item));
        next = std::move(ordered);
    }
    applySavedOrder(next);
    for (auto &item : next) {
        const auto old = std::find_if(items.cbegin(), items.cend(), [&item](const Item &candidate) { return candidate.key == item.key; });
        if (old != items.cend()) {
            if (!item.owner) { item.owner = old->owner; item.id = old->id; }
            if (!item.callback) item.callback = old->callback;
            if (!item.version) item.version = old->version;
            if (!item.icon) item.icon = old->icon;
            if (item.image.isNull()) item.image = old->image;
            if (item.iconImage.isNull()) item.iconImage = old->iconImage;
        }
        if (item.icon) {
            auto image = trayIconImage(item.icon);
            if (!image.isNull()) item.iconImage = std::move(image);
        }
    }
    QHash<qulonglong, QImage> nextIconImages;
    for (const auto &item : next) {
        auto image = !item.iconImage.isNull() ? item.iconImage : item.image;
        if (image.isNull() && item.owner) image = iconFromPath(processPath(reinterpret_cast<HWND>(static_cast<quintptr>(item.owner))), QSize(32, 32)).toImage();
        if (!image.isNull()) nextIconImages.insert(item.key, std::move(image));
    }
    if (next.size() == items.size()) {
        bool same = true;
        for (int i = 0; i < next.size(); ++i) if (next.at(i).key != items.at(i).key || next.at(i).owner != items.at(i).owner || next.at(i).id != items.at(i).id || next.at(i).callback != items.at(i).callback || next.at(i).version != items.at(i).version || next.at(i).icon != items.at(i).icon || next.at(i).tooltip != items.at(i).tooltip || next.at(i).automationId != items.at(i).automationId || next.at(i).image != items.at(i).image || next.at(i).iconImage != items.at(i).iconImage) { same = false; break; }
        if (same) {
            releaseAutomationItems(next);
            if (recover) ShowWindow(shell, SW_HIDE);
            return;
        }
    }
    beginResetModel();
    releaseAutomationItems(items);
    items = std::move(next);
    {
        QWriteLocker locker(&iconImageLock);
        iconImages = std::move(nextIconImages);
    }
    ++imageRevision;
    endResetModel();
    emit iconRevisionChanged();
    if (recover) ShowWindow(shell, SW_HIDE);
#endif
}

QPixmap TrayModel::icon(qulonglong key, const QSize &requestedSize) const
{
#ifdef Q_OS_WIN
    const auto requested = requestedSize.isValid() ? requestedSize : QSize(32, 32);
    QImage image;
    {
        QReadLocker locker(&iconImageLock);
        image = iconImages.value(key);
    }
    return image.isNull() ? QPixmap() : QPixmap::fromImage(image.scaled(requested, Qt::KeepAspectRatio, Qt::SmoothTransformation));
#else
    Q_UNUSED(key);
    Q_UNUSED(requestedSize);
    return {};
#endif
}

void TrayModel::showContextMenu(const QString &key, int x, int y)
{
#ifdef Q_OS_WIN
    bool ok = false;
    const auto value = key.toULongLong(&ok);
    const auto found = std::find_if(items.cbegin(), items.cend(), [value](const Item &item) { return item.key == value; });
    if (!ok || found == items.cend() || !found->owner || !found->callback) return;
    const auto owner = reinterpret_cast<HWND>(static_cast<quintptr>(found->owner));
    if (!IsWindow(owner)) return;
    DWORD processId = 0;
    GetWindowThreadProcessId(owner, &processId);
    AllowSetForegroundWindow(processId);
    const auto send = [&](UINT message) {
        WPARAM wParam = found->id;
        LPARAM lParam = MAKELPARAM(static_cast<WORD>(message), 0);
        if (found->version > 3) {
            wParam = MAKELPARAM(static_cast<WORD>(x), static_cast<WORD>(y));
            lParam = MAKELPARAM(static_cast<WORD>(message), static_cast<WORD>(found->id));
        }
        SendNotifyMessageW(owner, found->callback, wParam, lParam);
    };
    send(WM_RBUTTONDOWN);
    send(WM_RBUTTONUP);
    if (found->version >= 3) send(WM_CONTEXTMENU);
#else
    Q_UNUSED(key);
    Q_UNUSED(x);
    Q_UNUSED(y);
#endif
}

void TrayModel::activate(const QString &key, int action)
{
#ifdef Q_OS_WIN
    bool ok = false;
    const auto value = key.toULongLong(&ok);
    const auto found = std::find_if(items.cbegin(), items.cend(), [value](const Item &item) { return item.key == value; });
    if (!ok || found == items.cend()) return;
    if (found->automationElement) {
        if (action == 1) {
            POINT point{};
            GetCursorPos(&point);
            showContextMenu(key, point.x, point.y);
        } else if (action != 3) invokeAutomationItem(found->automationElement);
        return;
    }
    const auto owner = reinterpret_cast<HWND>(static_cast<quintptr>(found->owner));
    if (!IsWindow(owner)) return;
    if (action == 3) PostMessageW(owner, found->callback, found->id, WM_MOUSEMOVE);
    else if (action == 1) {
        POINT point{};
        GetCursorPos(&point);
        showContextMenu(key, point.x, point.y);
    } else if (action == 2) {
        PostMessageW(owner, found->callback, found->id, WM_LBUTTONDBLCLK);
    } else {
        PostMessageW(owner, found->callback, found->id, WM_LBUTTONDOWN);
        PostMessageW(owner, found->callback, found->id, WM_LBUTTONUP);
    }
#else
    Q_UNUSED(key);
    Q_UNUSED(action);
#endif
}

TrayFilterModel::TrayFilterModel(TrayModel *source, bool overflow, QObject *parent) : QSortFilterProxyModel(parent), tray(source), overflowOnly(overflow)
{
    setSourceModel(source);
    connect(source, &TrayModel::overflowChanged, this, &TrayFilterModel::refreshFilter);
    connect(this, &QAbstractItemModel::modelReset, this, &TrayFilterModel::countChanged);
}

bool TrayFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (!tray) return false;
    const auto key = tray->data(tray->index(sourceRow, 0, sourceParent), TrayModel::KeyRole).toString();
    return tray->isOverflow(key) == overflowOnly;
}

void TrayFilterModel::refreshFilter() { invalidateFilter(); emit countChanged(); }

void TrayFilterModel::move(int from, int to)
{
    if (!tray || from < 0 || to < 0 || from >= rowCount() || to >= rowCount() || from == to) return;
    QStringList orderedKeys;
    for (int row = 0; row < rowCount(); ++row) orderedKeys.append(data(index(row, 0), TrayModel::KeyRole).toString());
    orderedKeys.move(from, to);
    tray->reorder(orderedKeys, overflowOnly);
}
