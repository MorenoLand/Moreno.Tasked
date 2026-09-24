#include "windows_shell.h"
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "tray_icon_hook_client.h"
#include "tray_icon_hook_protocol.h"
#endif

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
#include <objbase.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <UIAutomation.h>
#include <oleauto.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <thread>
#include <utility>

namespace {
QHash<int, QByteArray> windowRoleNames() { return {{RunningAppsModel::TitleRole, "title"}, {RunningAppsModel::WindowRole, "windowHandle"}, {RunningAppsModel::IconSourceRole, "iconSource"}, {RunningAppsModel::ActiveRole, "active"}}; }
QHash<int, QByteArray> trayRoleNames() { return {{TrayModel::KeyRole, "key"}, {TrayModel::TooltipRole, "tooltip"}}; }

#ifdef Q_OS_WIN
HWND traySpyWindow = nullptr;
bool traySpyClassOwned = false;
QVector<TrayModel::Item> observedTrayItems;
QHash<qulonglong, QImage> xamlTrayImages;
struct XamlTrayImage { QString automationId; QString name; int ordinal = 0; QPoint screen; QImage image; };
QVector<XamlTrayImage> xamlTrayImageItems;
quint64 xamlTrayImageGeneration = 0;
qulonglong automationKey(const QString &automationId, const QString &name, int ordinal);

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
        const auto *copy = reinterpret_cast<const COPYDATASTRUCT *>(lParam);
        if (copy && copy->dwData == tasked::trayhook::copyDataTag && copy->lpData && copy->cbData >= sizeof(tasked::trayhook::IconPacketHeader)) {
            const auto *header = static_cast<const tasked::trayhook::IconPacketHeader *>(copy->lpData);
            const auto textBytes = static_cast<size_t>(header->automationIdLength) + header->nameLength;
            const auto pixelOffset = sizeof(*header) + textBytes;
            const auto pixelBytes = static_cast<size_t>(header->pixelBytes);
            if (header->magic == tasked::trayhook::packetMagic && header->version == tasked::trayhook::protocolVersion && header->width > 0 && header->height > 0 && header->width <= 128 && header->height <= 128 && header->stride >= header->width * 4 && pixelBytes >= static_cast<size_t>(header->stride) * header->height && pixelOffset <= copy->cbData && pixelBytes <= copy->cbData - pixelOffset) {
                if (header->requestId > xamlTrayImageGeneration) { xamlTrayImages.clear(); xamlTrayImageItems.clear(); xamlTrayImageGeneration = header->requestId; }
                if (header->requestId != xamlTrayImageGeneration) return TRUE;
                const auto *text = static_cast<const char *>(copy->lpData) + sizeof(*header);
                const auto automationId = QString::fromUtf8(text, static_cast<int>(header->automationIdLength));
                const auto name = QString::fromUtf8(text + header->automationIdLength, static_cast<int>(header->nameLength));
                const auto *pixels = reinterpret_cast<const uchar *>(static_cast<const char *>(copy->lpData) + pixelOffset);
                const QImage image(pixels, static_cast<int>(header->width), static_cast<int>(header->height), static_cast<int>(header->stride), QImage::Format_ARGB32_Premultiplied);
                if (!image.isNull() && (!automationId.isEmpty() || !name.isEmpty())) {
                    const auto ordinal = static_cast<int>(header->ordinal);
                    const auto key = automationKey(automationId, name, ordinal);
                    const auto copyImage = image.copy();
                    xamlTrayImages.insert(key, copyImage);
                    const QPoint screen(header->screenX, header->screenY);
                    const auto existing = std::find_if(xamlTrayImageItems.begin(), xamlTrayImageItems.end(), [&automationId, &name, ordinal](const XamlTrayImage &candidate) { return candidate.automationId == automationId && candidate.name == name && candidate.ordinal == ordinal; });
                    if (existing == xamlTrayImageItems.end()) xamlTrayImageItems.append({automationId, name, ordinal, screen, copyImage});
                    else { existing->screen = screen; existing->image = copyImage; }
                    if (xamlTrayImageItems.size() > 256) xamlTrayImageItems.removeFirst();
                }
                return TRUE;
            }
            return FALSE;
        }
        observeTrayMessage(copy);
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
    WNDCLASSEXW windowClass{ sizeof(WNDCLASSEXW), 0, traySpyWindowProc, 0, 0, instance, nullptr, nullptr, nullptr, nullptr, L"Tasked.TraySpyWindow", nullptr };
    if (!RegisterClassExW(&windowClass)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
    } else traySpyClassOwned = true;
    traySpyWindow = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"Tasked.TraySpyWindow", nullptr, WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (!traySpyWindow) {
        if (traySpyClassOwned) UnregisterClassW(L"Tasked.TraySpyWindow", instance);
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
        UnregisterClassW(L"Tasked.TraySpyWindow", GetModuleHandleW(nullptr));
        traySpyClassOwned = false;
    }
    observedTrayItems.clear();
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

struct ShellNotifyItem { PWSTR exeName; PWSTR tooltip; HICON icon; HWND owner; DWORD preference; UINT id; GUID guid; };

struct __declspec(uuid("D782CCBA-AFB0-43F1-94DB-FDA3779EACCB")) ITaskedTrayNotifyCallback : IUnknown { virtual HRESULT STDMETHODCALLTYPE Notify(ULONG event, ShellNotifyItem *item) = 0; };
struct __declspec(uuid("D133CE13-3537-48BA-93A7-AFCD5D2053B4")) ITaskedTrayNotifyWin8 : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE RegisterCallback(ITaskedTrayNotifyCallback *callback, ULONG *cookie) = 0;
    virtual HRESULT STDMETHODCALLTYPE UnregisterCallback(ULONG *cookie) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPreference(const ShellNotifyItem *item) = 0;
    virtual HRESULT STDMETHODCALLTYPE EnableAutoTray(BOOL enabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE DoAction(BOOL action) = 0;
};

class TrayNotifySnapshot final : public ITaskedTrayNotifyCallback {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override { if (!object) return E_POINTER; *object = nullptr; if (iid == IID_IUnknown || iid == __uuidof(ITaskedTrayNotifyCallback)) *object = static_cast<ITaskedTrayNotifyCallback *>(this); else return E_NOINTERFACE; AddRef(); return S_OK; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override { return --references; }
    HRESULT STDMETHODCALLTYPE Notify(ULONG, ShellNotifyItem *item) override {
        if (!item || !item->owner || !item->icon) return S_OK;
        TrayModel::Item trayItem;
        trayItem.owner = reinterpret_cast<quintptr>(item->owner);
        trayItem.id = item->id;
        trayItem.icon = reinterpret_cast<quintptr>(item->icon);
        trayItem.tooltip = item->tooltip ? QString::fromWCharArray(item->tooltip) : QString();
        trayItem.key = static_cast<qulonglong>(trayItem.owner) ^ (static_cast<qulonglong>(trayItem.id) << 32);
        trayItem.iconImage = trayIconImage(trayItem.icon);
        NOTIFYICONIDENTIFIER identifier{};
        identifier.cbSize = sizeof(identifier);
        identifier.hWnd = item->owner;
        identifier.uID = item->id;
        identifier.guidItem = item->guid;
        RECT bounds{};
        if (SUCCEEDED(Shell_NotifyIconGetRect(&identifier, &bounds))) trayItem.bounds = QRect(bounds.left, bounds.top, bounds.right - bounds.left, bounds.bottom - bounds.top);
        items.append(std::move(trayItem));
        return S_OK;
    }
    QVector<TrayModel::Item> takeItems() { return std::move(items); }
private:
    ULONG references = 1;
    QVector<TrayModel::Item> items;
};

QVector<TrayModel::Item> trayNotifyItems()
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return {};
    const CLSID clsid{0x25dead04, 0x1eac, 0x4911, {0x9e, 0x3a, 0xad, 0x0a, 0x4a, 0xb5, 0x60, 0xfd}};
    ITaskedTrayNotifyWin8 *trayNotify = nullptr;
    auto items = QVector<TrayModel::Item>();
    if (SUCCEEDED(CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER, __uuidof(ITaskedTrayNotifyWin8), reinterpret_cast<void **>(&trayNotify)))) {
        TrayNotifySnapshot snapshot;
        ULONG cookie = 0;
        if (SUCCEEDED(trayNotify->RegisterCallback(&snapshot, &cookie))) {
            trayNotify->UnregisterCallback(&cookie);
            items = snapshot.takeItems();
        }
        trayNotify->Release();
    }
    if (SUCCEEDED(initialized)) CoUninitialize();
    return items;
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

QVector<TrayModel::Item> automationTrayItems()
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

bool sendWindowCommand(HWND window, WPARAM command)
{
    DWORD_PTR result = 0;
    return window && SendMessageTimeoutW(window, WM_SYSCOMMAND, command, 0, SMTO_ABORTIFHUNG, 1000, &result) != 0;
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
        if (!ReadProcessMemory(process, reinterpret_cast<const void *>(button.dwData), &tray, sizeof(tray), nullptr) || !tray.owner) continue;
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
        auto found = native.cend();
        if (item.owner) found = std::find_if(native.cbegin(), native.cend(), [&item, &used, &native](const TrayModel::Item &candidate) { const auto index = static_cast<int>(&candidate - native.constData()); return !used.contains(index) && candidate.owner == item.owner && candidate.id == item.id && candidate.icon; });
        if (found == native.cend() && !item.tooltip.isEmpty()) found = std::find_if(native.cbegin(), native.cend(), [&item, &used, &native](const TrayModel::Item &candidate) { const auto index = static_cast<int>(&candidate - native.constData()); return !used.contains(index) && candidate.icon && !candidate.tooltip.isEmpty() && candidate.tooltip.compare(item.tooltip, Qt::CaseInsensitive) == 0; });
        if (found == native.cend() && item.bounds.isValid()) {
        qint64 bestDistance = 0;
        const auto itemCenter = item.bounds.center();
        for (auto candidate = native.cbegin(); candidate != native.cend(); ++candidate) {
            const auto index = static_cast<int>(&*candidate - native.constData());
            if (used.contains(index) || !candidate->icon || !candidate->bounds.isValid() || (!item.bounds.intersects(candidate->bounds) && !item.bounds.contains(candidate->bounds.center()) && !candidate->bounds.contains(itemCenter))) continue;
            const auto deltaX = static_cast<qint64>(itemCenter.x()) - candidate->bounds.center().x();
            const auto deltaY = static_cast<qint64>(itemCenter.y()) - candidate->bounds.center().y();
            const auto distance = deltaX * deltaX + deltaY * deltaY;
            if (found != native.cend() && distance >= bestDistance) continue;
            found = candidate;
            bestDistance = distance;
        }
        }
        if (found == native.cend()) continue;
        const auto index = static_cast<int>(&(*found) - native.constData());
        used.insert(index);
        item.owner = found->owner;
        item.id = found->id;
        item.callback = found->callback;
        item.version = found->version;
        item.icon = found->icon;
        if (!found->iconImage.isNull()) item.iconImage = found->iconImage;
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
    activate(windowHandle, false);
}

void RunningAppsModel::activate(const QString &windowHandle, bool wasActiveAtPress)
{
#ifdef Q_OS_WIN
    bool ok = false;
    const auto value = windowHandle.toULongLong(&ok);
    const auto window = reinterpret_cast<HWND>(static_cast<quintptr>(value));
    if (ok && IsWindow(window)) {
        const auto root = GetAncestor(window, GA_ROOT);
        const auto target = root ? root : window;
        const auto foreground = GetForegroundWindow();
        const auto foregroundRoot = foreground ? GetAncestor(foreground, GA_ROOT) : nullptr;
        const auto dock = FindWindowW(nullptr, L"Tasked");
        const auto dockRoot = dock ? GetAncestor(dock, GA_ROOT) : nullptr;
        const auto cachedActive = std::any_of(items.cbegin(), items.cend(), [window](const Item &item) { return item.window == static_cast<qulonglong>(reinterpret_cast<quintptr>(window)) && item.active; });
        const auto foregroundTask = foreground == target || foregroundRoot == target;
        const auto dockWasForeground = foreground == dock || foregroundRoot == dockRoot;
        if (!IsIconic(target) && (foregroundTask || (dockWasForeground && (cachedActive || wasActiveAtPress)))) {
            sendWindowCommand(target, SC_MINIMIZE);
            if (!IsIconic(target)) ShowWindow(target, SW_MINIMIZE);
            return;
        }
        if (IsIconic(target)) {
            sendWindowCommand(target, SC_RESTORE);
            if (IsIconic(target)) ShowWindow(target, SW_RESTORE);
        }
        const auto targetThread = GetWindowThreadProcessId(target, nullptr);
        const auto currentThread = GetCurrentThreadId();
        DWORD targetProcess = 0;
        GetWindowThreadProcessId(target, &targetProcess);
        AllowSetForegroundWindow(targetProcess);
        const auto attached = targetThread && targetThread != currentThread && AttachThreadInput(currentThread, targetThread, TRUE);
        BringWindowToTop(target);
        SetForegroundWindow(target);
        if (GetForegroundWindow() != target) SwitchToThisWindow(target, TRUE);
        if (attached) SetFocus(target);
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
    if (command) sendWindowCommand(window, static_cast<WPARAM>(command));
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
    tasked::trayhook::startClient(traySpyWindow);
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
    tasked::trayhook::stopClient();
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
    if (shell) tasked::trayhook::requestScan(shell);
    const auto visible = shell && IsWindowVisible(shell);
    QVector<Item> next = automationTrayItems();
    QVector<Item> native = trayNotifyItems();
    for (const auto toolbar : trayToolbars()) enumerateToolbar(toolbar, native);
    if (next.isEmpty()) {
        next = std::move(native);
        if (next.isEmpty() && !visible) {
            return;
        }
    }
    applyObservedTrayItems(next);
    if (!native.isEmpty()) enrichTrayItems(next, native);
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
            if (item.iconImage.isNull()) item.iconImage = old->iconImage;
        }
        if (item.icon) {
            auto image = trayIconImage(item.icon);
            if (!image.isNull()) item.iconImage = std::move(image);
        }
        const auto liveImage = xamlTrayImages.value(item.key);
        if (!liveImage.isNull()) item.iconImage = liveImage;
        else if (item.iconImage.isNull() && item.bounds.isValid()) {
            const auto itemCenter = item.bounds.center();
            auto nearest = xamlTrayImageItems.cend();
            qint64 nearestDistance = 0;
            for (auto candidate = xamlTrayImageItems.cbegin(); candidate != xamlTrayImageItems.cend(); ++candidate) {
                const QPoint iconCenter(candidate->screen.x() + 8, candidate->screen.y() + 8);
                const auto deltaX = static_cast<qint64>(itemCenter.x()) - iconCenter.x();
                const auto deltaY = static_cast<qint64>(itemCenter.y()) - iconCenter.y();
                const auto distance = deltaX * deltaX + deltaY * deltaY;
                if (!item.bounds.contains(iconCenter) && distance > 256) continue;
                if (nearest != xamlTrayImageItems.cend() && distance >= nearestDistance) continue;
                nearest = candidate;
                nearestDistance = distance;
            }
            if (nearest != xamlTrayImageItems.cend()) item.iconImage = nearest->image;
        }
    }
    QHash<qulonglong, QImage> nextIconImages;
    for (const auto &item : next) {
        if (!item.iconImage.isNull()) nextIconImages.insert(item.key, item.iconImage);
    }
    if (next.size() == items.size()) {
        bool same = true;
        for (int i = 0; i < next.size(); ++i) if (next.at(i).key != items.at(i).key || next.at(i).owner != items.at(i).owner || next.at(i).id != items.at(i).id || next.at(i).callback != items.at(i).callback || next.at(i).version != items.at(i).version || next.at(i).icon != items.at(i).icon || next.at(i).tooltip != items.at(i).tooltip || next.at(i).automationId != items.at(i).automationId || next.at(i).iconImage != items.at(i).iconImage) { same = false; break; }
        if (same) {
            releaseAutomationItems(next);
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
        if (action == 1) { POINT point{}; GetCursorPos(&point); showContextMenu(key, point.x, point.y); return; }
        if (action == 3 || invokeAutomationItem(found->automationElement)) return;
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
    } else if (found->version >= 4) {
        POINT point{};
        GetCursorPos(&point);
        PostMessageW(owner, found->callback, MAKELPARAM(static_cast<WORD>(point.x), static_cast<WORD>(point.y)), MAKELPARAM(NIN_SELECT, static_cast<WORD>(found->id)));
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
