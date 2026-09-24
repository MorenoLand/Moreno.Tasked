#define NOMINMAX
#include "tray_icon_hook_protocol.h"
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <unknwn.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace winrt;
namespace xaml = winrt::Windows::UI::Xaml;
namespace streams = winrt::Windows::Storage::Streams;
using tasked::trayhook::IconPacketHeader;
using tasked::trayhook::SharedRequest;
using tasked::trayhook::iconPacketKind;
using tasked::trayhook::maxIconPixelBytes;
using tasked::trayhook::maxIconSide;
using tasked::trayhook::scanCompletePacketKind;
struct ScanRequest { DWORD hostPid; HWND targetWindow; std::uint32_t generation; SharedRequest shared; };
struct IconPacket { std::vector<std::uint8_t> bytes; };
struct ScanCapture { std::vector<IconPacket> packets; bool complete = false; };
struct TrayItem { xaml::FrameworkElement view{nullptr}; xaml::FrameworkElement content{nullptr}; std::string automationId; std::string name; RECT screenBounds{}; std::uint32_t ordinal{}; };
volatile LONG scanActive = 0;
constexpr std::size_t maxTreeNodes = 16384;
constexpr std::size_t maxStringBytes = 2048;
constexpr DWORD sendTimeoutMs = 250;

UINT ScanMessage() noexcept
{
    static const UINT value = RegisterWindowMessageW(L"Tasked.TrayHook.Scan.v1");
    return value;
}

bool ReadCurrent(const void* address, void* destination, SIZE_T size) noexcept
{
    SIZE_T read = 0;
    return address && destination && size && ReadProcessMemory(GetCurrentProcess(), address, destination, size, &read) && read == size;
}

bool Readable(const void* address, SIZE_T size) noexcept
{
    if (!address || !size) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(address, &info, sizeof(info)) || info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto region = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
    return start >= region && size <= info.RegionSize && start - region <= info.RegionSize - size;
}

bool ImageRange(const std::uint8_t* base, std::uint32_t imageSize, std::uint64_t rva, std::size_t size) noexcept
{
    return base && rva <= imageSize && size <= imageSize - static_cast<std::size_t>(rva);
}

bool ExecutableAddress(const void* address) noexcept
{
    MEMORY_BASIC_INFORMATION info{};
    if (!address || !VirtualQuery(address, &info, sizeof(info)) || info.State != MEM_COMMIT || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const DWORD protection = info.Protect & 0xff;
    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

bool ValidateTaskbarImage(const SharedRequest& request, std::uint8_t** baseOut) noexcept
{
#if defined(_M_X64)
    constexpr WORD expectedMachine = IMAGE_FILE_MACHINE_AMD64;
#elif defined(_M_ARM64)
    constexpr WORD expectedMachine = IMAGE_FILE_MACHINE_ARM64;
#else
    return false;
#endif
    HMODULE module = GetModuleHandleW(L"taskbar.dll");
    if (!module) return false;
    auto* base = reinterpret_cast<std::uint8_t*>(module);
    IMAGE_DOS_HEADER dos{};
    if (!ReadCurrent(base, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < static_cast<LONG>(sizeof(dos)) || dos.e_lfanew > 0x100000) return false;
    IMAGE_NT_HEADERS64 nt{};
    if (!ReadCurrent(base + dos.e_lfanew, &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != expectedMachine || nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) || nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
    if (nt.FileHeader.TimeDateStamp != request.taskbarTimeDateStamp || nt.OptionalHeader.SizeOfImage != request.taskbarSizeOfImage || !nt.OptionalHeader.SizeOfImage) return false;
    if (!ImageRange(base, nt.OptionalHeader.SizeOfImage, request.getTaskbarHostRva, 1) || !ImageRange(base, nt.OptionalHeader.SizeOfImage, request.frameHeightRva, 8) || !ImageRange(base, nt.OptionalHeader.SizeOfImage, request.refCountDecrefRva, 1)) return false;
    const auto getHost = base + request.getTaskbarHostRva;
    const auto frameHeight = base + request.frameHeightRva;
    const auto decref = base + request.refCountDecrefRva;
    if (!ExecutableAddress(getHost) || !ExecutableAddress(frameHeight) || !ExecutableAddress(decref)) return false;
    *baseOut = base;
    return true;
}

bool ResolveElementOffset(const std::uint8_t* base, const SharedRequest& request, std::uint64_t* offset) noexcept
{
    if (!base || !offset) return false;
#if defined(_M_X64)
    std::uint8_t bytes[8]{};
    if (!ReadCurrent(base + request.frameHeightRva, bytes, sizeof(bytes)) || bytes[0] != 0x48 || bytes[1] != 0x83 || bytes[2] != 0xEC || bytes[4] != 0x48 || bytes[5] != 0x83 || bytes[6] != 0xC1 || !bytes[7] || bytes[7] > 0x7F || bytes[7] % sizeof(void*)) return false;
    *offset = bytes[7];
    return true;
#elif defined(_M_ARM64)
    DWORD instructions[4]{};
    if (!ReadCurrent(base + request.frameHeightRva, instructions, sizeof(instructions)) || instructions[0] != 0xD503237F || (instructions[1] & 0xFFC07FFF) != 0xA9807BFD || instructions[2] != 0x910003FD || (instructions[3] & 0xFFF00FE0) != 0xF8400C00) return false;
    *offset = (instructions[3] >> 12) & 0xFF;
    return *offset && *offset <= 0x7F && *offset % sizeof(void*) == 0;
#else
    return false;
#endif
}

bool IsTaskbarWindow(HWND window) noexcept
{
    DWORD pid = 0;
    wchar_t className[64]{};
    return window && GetWindowThreadProcessId(window, &pid) && pid == GetCurrentProcessId() && GetClassNameW(window, className, static_cast<int>(std::size(className))) && _wcsicmp(className, L"Shell_TrayWnd") == 0;
}

HWND FindTaskbarWindow() noexcept
{
    HWND result = nullptr;
    EnumWindows([](HWND window, LPARAM parameter) -> BOOL {
        if (IsTaskbarWindow(window)) { *reinterpret_cast<HWND *>(parameter) = window; return FALSE; }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&result));
    return result;
}

using GetTaskbarHostFn = void* (WINAPI*)(void*, void*);
using DecrefFn = void (WINAPI*)(void*);

bool InvokeGetTaskbarHost(void* fn, void* object, void** result) noexcept
{
    __try { reinterpret_cast<GetTaskbarHostFn>(fn)(object, result); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool VtableContainsGetTaskbarHost(const std::uint8_t* base, std::uint32_t imageSize, void* vtable, void* getHost) noexcept
{
    const auto baseAddress = reinterpret_cast<std::uintptr_t>(base);
    const auto tableAddress = reinterpret_cast<std::uintptr_t>(vtable);
    constexpr std::size_t taskListWndSiteGetTaskbarHostIndex = 28;
    if (!base || !vtable || tableAddress < baseAddress || tableAddress - baseAddress >= imageSize) return false;
    const auto tableRva = static_cast<std::uint64_t>(tableAddress - baseAddress);
    for (std::size_t index = 0; index < 96; ++index) {
        const auto entryRva = tableRva + index * sizeof(void*);
        if (!ImageRange(base, imageSize, entryRva, sizeof(void*))) return false;
        void* entry = nullptr;
        if (!ReadCurrent(base + entryRva, &entry, sizeof(entry)) || !entry) return false;
        if (entry == getHost) return index == taskListWndSiteGetTaskbarHostIndex;
    }
    return false;
}

void InvokeDecref(void* fn, void* controlBlock) noexcept
{
    if (!fn || !controlBlock) return;
    __try { reinterpret_cast<DecrefFn>(fn)(controlBlock); }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}

HRESULT QueryInterfaceSafe(IUnknown* object, REFIID iid, void** result) noexcept
{
    __try { return object ? object->QueryInterface(iid, result) : E_POINTER; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return E_FAIL; }
}

struct HostRelease { void* fn{}; void* controlBlock{}; ~HostRelease() { InvokeDecref(fn, controlBlock); } };

xaml::XamlRoot GetTaskbarXamlRoot(HWND taskbarWindow, const SharedRequest& request)
{
    std::uint8_t* base = nullptr;
    if (!ValidateTaskbarImage(request, &base) || !IsTaskbarWindow(taskbarWindow)) return nullptr;
    HWND taskbandWindow = reinterpret_cast<HWND>(GetPropW(taskbarWindow, L"TaskbandHWND"));
    DWORD ownerPid = 0;
    if (!taskbandWindow || !GetWindowThreadProcessId(taskbandWindow, &ownerPid) || ownerPid != GetCurrentProcessId()) return nullptr;
    void* taskband = reinterpret_cast<void*>(GetWindowLongPtrW(taskbandWindow, 0));
    if (!taskband) return nullptr;
    void* taskbandForSite = nullptr;
    auto* getHost = base + request.getTaskbarHostRva;
    for (std::size_t i = 0; i < 20; ++i) {
        void* candidate = reinterpret_cast<std::uint8_t*>(taskband) + i * sizeof(void*);
        void* vtable = nullptr;
        if (!ReadCurrent(candidate, &vtable, sizeof(vtable))) return nullptr;
        if (VtableContainsGetTaskbarHost(base, request.taskbarSizeOfImage, vtable, getHost)) { taskbandForSite = candidate; break; }
    }
    if (!taskbandForSite) return nullptr;
    void* shared[2]{};
    void* decref = base + request.refCountDecrefRva;
    if (!InvokeGetTaskbarHost(getHost, taskbandForSite, shared)) return nullptr;
    HostRelease release{decref, shared[1]};
    if (!shared[0] || !shared[1] || !Readable(shared[0], 1)) return nullptr;
    std::uint64_t elementOffset = 0;
    if (!ResolveElementOffset(base, request, &elementOffset) || !Readable(reinterpret_cast<std::uint8_t*>(shared[0]) + elementOffset, sizeof(IUnknown*))) return nullptr;
    auto* field = reinterpret_cast<std::uint8_t*>(shared[0]) + elementOffset;
    IUnknown* elementUnknown = nullptr;
    if (!ReadCurrent(field, &elementUnknown, sizeof(elementUnknown)) || !elementUnknown) return nullptr;
    xaml::FrameworkElement taskbarElement{nullptr};
    if (FAILED(QueryInterfaceSafe(elementUnknown, winrt::guid_of<xaml::FrameworkElement>(), winrt::put_abi(taskbarElement))) || !taskbarElement) return nullptr;
    return taskbarElement.XamlRoot();
}

xaml::FrameworkElement FindFrameGrid(const xaml::DependencyObject& root)
{
    std::vector<xaml::DependencyObject> pending{root};
    for (std::size_t index = 0; index < pending.size() && index < maxTreeNodes; ++index) {
        const auto current = pending[index];
        if (auto element = current.try_as<xaml::FrameworkElement>(); element && element.Name() == L"SystemTrayFrameGrid") return element;
        const int count = xaml::Media::VisualTreeHelper::GetChildrenCount(current);
        if (count < 0 || count > 2048 || pending.size() + static_cast<std::size_t>(count) > maxTreeNodes) return nullptr;
        for (int i = 0; i < count; ++i) pending.push_back(xaml::Media::VisualTreeHelper::GetChild(current, i));
    }
    return nullptr;
}

bool IsIconView(const xaml::FrameworkElement& element)
{
    const auto className = winrt::get_class_name(element);
    return className == L"SystemTray.NotifyIconView" || className == L"SystemTray.IconView";
}

bool IsIconContent(const xaml::FrameworkElement& element)
{
    const auto className = winrt::get_class_name(element);
    const auto name = element.Name();
    return className == L"SystemTray.ImageIconContent" || className == L"SystemTray.TextIconContent" || name == L"ImageIconContent" || name == L"TextIconContent";
}

std::vector<xaml::FrameworkElement> FindIconContent(const xaml::FrameworkElement& view)
{
    std::vector<xaml::DependencyObject> pending{view};
    std::vector<xaml::FrameworkElement> found;
    for (std::size_t index = 0; index < pending.size() && index < 1024; ++index) {
        const auto current = pending[index];
        if (auto element = current.try_as<xaml::FrameworkElement>(); element && IsIconContent(element)) {
            if (element.Visibility() == xaml::Visibility::Visible && element.Opacity() > 0.0 && element.ActualWidth() > 0.0 && element.ActualHeight() > 0.0) found.push_back(element);
            continue;
        }
        const int count = xaml::Media::VisualTreeHelper::GetChildrenCount(current);
        if (count < 0 || count > 256 || pending.size() + static_cast<std::size_t>(count) > 1024) return {};
        for (int i = 0; i < count; ++i) pending.push_back(xaml::Media::VisualTreeHelper::GetChild(current, i));
    }
    return found;
}

bool ToScreenRect(const xaml::FrameworkElement& view, const xaml::FrameworkElement& root, const xaml::XamlRoot& xamlRoot, HWND taskbarWindow, RECT* screenBounds)
{
    if (!screenBounds) return false;
    POINT origin{};
    if (!ClientToScreen(taskbarWindow, &origin)) return false;
    const auto point = view.TransformToVisual(root).TransformPoint({0.0, 0.0});
    const double scale = xamlRoot.RasterizationScale();
    const double x = origin.x + point.X * scale;
    const double y = origin.y + point.Y * scale;
    const double width = view.ActualWidth() * scale;
    const double height = view.ActualHeight() * scale;
    const double right = x + width;
    const double bottom = y + height;
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(width) || !std::isfinite(height) || !std::isfinite(right) || !std::isfinite(bottom) || width <= 0.0 || height <= 0.0 || x < std::numeric_limits<LONG>::min() || x > std::numeric_limits<LONG>::max() || y < std::numeric_limits<LONG>::min() || y > std::numeric_limits<LONG>::max() || right < std::numeric_limits<LONG>::min() || right > std::numeric_limits<LONG>::max() || bottom < std::numeric_limits<LONG>::min() || bottom > std::numeric_limits<LONG>::max()) return false;
    *screenBounds = RECT{static_cast<LONG>(std::lround(x)), static_cast<LONG>(std::lround(y)), static_cast<LONG>(std::lround(right)), static_cast<LONG>(std::lround(bottom))};
    if (screenBounds->right <= screenBounds->left || screenBounds->bottom <= screenBounds->top) return false;
    return true;
}

bool ValidText(const std::string& value) noexcept
{
    return value.size() <= tasked::trayhook::maxPacketTextBytes && value.find('\0') == std::string::npos;
}

bool BuildPacket(const TrayItem& item, std::uint32_t generation, std::uint32_t width, std::uint32_t height, const std::vector<std::uint8_t>& pixels, IconPacket* packet)
{
    if (!packet || !width || !height || width > maxIconSide || height > maxIconSide || item.automationId.empty() && item.name.empty() || !ValidText(item.automationId) || !ValidText(item.name)) return false;
    const std::uint64_t stride = static_cast<std::uint64_t>(width) * 4;
    const std::uint64_t pixelBytes = stride * height;
    if (stride > std::numeric_limits<std::uint32_t>::max() || pixelBytes > maxIconPixelBytes || pixels.size() != pixelBytes) return false;
    const std::uint64_t total = sizeof(IconPacketHeader) + item.automationId.size() + item.name.size() + pixelBytes;
    if (total > std::numeric_limits<DWORD>::max()) return false;
    IconPacketHeader header{};
    header.magic = tasked::trayhook::packetMagic;
    header.version = tasked::trayhook::protocolVersion;
    header.requestId = generation;
    header.automationIdLength = static_cast<std::uint32_t>(item.automationId.size());
    header.nameLength = static_cast<std::uint32_t>(item.name.size());
    header.ordinal = item.ordinal;
    header.screenX = item.screenBounds.left;
    header.screenY = item.screenBounds.top;
    header.screenWidth = static_cast<std::uint32_t>(item.screenBounds.right - item.screenBounds.left);
    header.screenHeight = static_cast<std::uint32_t>(item.screenBounds.bottom - item.screenBounds.top);
    header.width = width;
    header.height = height;
    header.stride = static_cast<std::uint32_t>(stride);
    header.pixelBytes = static_cast<std::uint32_t>(pixelBytes);
    header.packetKind = iconPacketKind;
    packet->bytes.resize(static_cast<std::size_t>(total));
    auto* output = packet->bytes.data();
    std::memcpy(output, &header, sizeof(header));
    output += sizeof(header);
    std::memcpy(output, item.automationId.data(), item.automationId.size());
    output += item.automationId.size();
    std::memcpy(output, item.name.data(), item.name.size());
    output += item.name.size();
    std::memcpy(output, pixels.data(), pixels.size());
    return true;
}

bool HasVisiblePixels(const std::vector<std::uint8_t>& pixels) noexcept
{
    for (std::size_t index = 3; index < pixels.size(); index += 4) if (pixels[index]) return true;
    return false;
}

winrt::Windows::Foundation::IAsyncAction CaptureIconsAsync(ScanRequest request, const std::shared_ptr<ScanCapture>& output)
{
    auto& packets = output->packets;
    HWND taskbarWindow = FindTaskbarWindow();
    if (!taskbarWindow) co_return;
    const auto xamlRoot = GetTaskbarXamlRoot(taskbarWindow, request.shared);
    if (!xamlRoot) co_return;
    const auto root = xamlRoot.Content().try_as<xaml::FrameworkElement>();
    if (!root) co_return;
    const auto frameGrid = FindFrameGrid(root);
    if (!frameGrid) co_return;
    std::vector<xaml::FrameworkElement> pending;
    const int gridChildren = xaml::Media::VisualTreeHelper::GetChildrenCount(frameGrid);
    if (gridChildren < 0 || gridChildren > 1024) co_return;
    for (int i = 0; i < gridChildren; ++i) pending.push_back(xaml::Media::VisualTreeHelper::GetChild(frameGrid, i).try_as<xaml::FrameworkElement>());
    std::vector<xaml::FrameworkElement> views;
    for (std::size_t index = 0; index < pending.size() && index < maxTreeNodes; ++index) {
        const auto element = pending[index];
        if (!element) continue;
        if (IsIconView(element) && element.Visibility() == xaml::Visibility::Visible && element.Opacity() > 0.0) views.push_back(element);
        const int count = xaml::Media::VisualTreeHelper::GetChildrenCount(element);
        if (count < 0 || count > 2048 || pending.size() + static_cast<std::size_t>(count) > maxTreeNodes) co_return;
        for (int i = 0; i < count; ++i) pending.push_back(xaml::Media::VisualTreeHelper::GetChild(element, i).try_as<xaml::FrameworkElement>());
    }
    std::vector<TrayItem> items;
    std::set<void*> contentObjects;
    for (const auto& view : views) {
        auto contents = FindIconContent(view);
        if (contents.size() != 1) continue;
        if (!contentObjects.insert(winrt::get_abi(contents.front())).second) continue;
        auto id = winrt::to_string(xaml::Automation::AutomationProperties::GetAutomationId(view));
        auto name = winrt::to_string(xaml::Automation::AutomationProperties::GetName(view));
        if (name.empty()) name = winrt::to_string(view.Name());
        if ((id.empty() && name.empty()) || !ValidText(id) || !ValidText(name)) continue;
        RECT screenBounds{};
        if (!ToScreenRect(view, root, xamlRoot, taskbarWindow, &screenBounds)) continue;
        items.push_back(TrayItem{view, contents.front(), std::move(id), std::move(name), screenBounds, 0});
    }
    if (items.size() > tasked::trayhook::maxTrayItems) co_return;
    std::map<std::pair<std::string, std::string>, std::uint32_t> ordinals;
    for (auto& item : items) item.ordinal = ordinals[{item.automationId, item.name}]++;
    bool complete = true;
    for (const auto& item : items) {
        try {
            if (item.content.Visibility() != xaml::Visibility::Visible || item.content.ActualWidth() <= 0.0 || item.content.ActualHeight() <= 0.0) { complete = false; break; }
            xaml::Media::Imaging::RenderTargetBitmap bitmap;
            const auto captureScale = (std::max)(1.0, xamlRoot.RasterizationScale()) * 2.0;
            const auto requestedWidth = (std::min)(static_cast<int>(std::ceil(item.content.ActualWidth() * captureScale)), static_cast<int>(maxIconSide));
            const auto requestedHeight = (std::min)(static_cast<int>(std::ceil(item.content.ActualHeight() * captureScale)), static_cast<int>(maxIconSide));
            co_await bitmap.RenderAsync(item.content, requestedWidth, requestedHeight);
            const auto width = bitmap.PixelWidth();
            const auto height = bitmap.PixelHeight();
            if (width <= 0 || height <= 0 || static_cast<std::uint32_t>(width) > maxIconSide || static_cast<std::uint32_t>(height) > maxIconSide) { complete = false; break; }
            const std::uint64_t expected = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) * 4;
            if (!expected || expected > maxIconPixelBytes) { complete = false; break; }
            const auto buffer = co_await bitmap.GetPixelsAsync();
            if (!buffer || buffer.Length() != expected) { complete = false; break; }
            auto reader = streams::DataReader::FromBuffer(buffer);
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(expected));
            reader.ReadBytes(winrt::array_view<std::uint8_t>(pixels.data(), pixels.data() + pixels.size()));
            if (!HasVisiblePixels(pixels)) continue;
            IconPacket packet;
            if (!BuildPacket(item, request.generation, static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), pixels, &packet)) { complete = false; break; }
            packets.push_back(std::move(packet));
        } catch (...) { complete = false; break; }
    }
    output->complete = complete;
}

bool SendPackets(const ScanRequest& request, const std::vector<IconPacket>& packets) noexcept
{
    DWORD pid = 0;
    if (!IsWindow(request.targetWindow) || !GetWindowThreadProcessId(request.targetWindow, &pid) || pid != request.hostPid) return false;
    for (const auto& packet : packets) {
        if (packet.bytes.empty() || packet.bytes.size() > std::numeric_limits<DWORD>::max()) return false;
        COPYDATASTRUCT data{};
        data.dwData = tasked::trayhook::copyDataTag;
        data.cbData = static_cast<DWORD>(packet.bytes.size());
        data.lpData = const_cast<std::uint8_t*>(packet.bytes.data());
        DWORD_PTR response = 0;
        if (!SendMessageTimeoutW(request.targetWindow, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data), SMTO_ABORTIFHUNG | SMTO_BLOCK, sendTimeoutMs, &response) || !response) return false;
    }
    return true;
}

bool SendScanComplete(const ScanRequest& request, std::uint32_t packetCount) noexcept
{
    DWORD pid = 0;
    if (!IsWindow(request.targetWindow) || !GetWindowThreadProcessId(request.targetWindow, &pid) || pid != request.hostPid) return false;
    IconPacketHeader header{};
    header.magic = tasked::trayhook::packetMagic;
    header.version = tasked::trayhook::protocolVersion;
    header.requestId = request.generation;
    header.packetKind = scanCompletePacketKind;
    header.expectedPacketCount = packetCount;
    COPYDATASTRUCT data{};
    data.dwData = tasked::trayhook::copyDataTag;
    data.cbData = sizeof(header);
    data.lpData = &header;
    DWORD_PTR response = 0;
    return SendMessageTimeoutW(request.targetWindow, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data), SMTO_ABORTIFHUNG | SMTO_BLOCK, sendTimeoutMs, &response) != 0;
}

winrt::fire_and_forget RunScanAsync(ScanRequest request)
{
    struct ScanRelease { ~ScanRelease() { InterlockedExchange(&scanActive, 0); } } release;
    auto capture = std::make_shared<ScanCapture>();
    try {
        co_await CaptureIconsAsync(request, capture);
    } catch (...) { capture->complete = false; }
    if (!capture->complete) co_return;
    co_await winrt::resume_background();
    if (capture->packets.size() <= std::numeric_limits<std::uint32_t>::max() && SendPackets(request, capture->packets)) SendScanComplete(request, static_cast<std::uint32_t>(capture->packets.size()));
}

bool ReadRequest(WPARAM hostPidValue, LPARAM generationValue, ScanRequest* result) noexcept
{
    if (!result || static_cast<ULONG_PTR>(hostPidValue) > MAXDWORD) return false;
    const DWORD hostPid = static_cast<DWORD>(hostPidValue);
    const auto generation = static_cast<std::uint32_t>(static_cast<ULONG_PTR>(generationValue));
    if (!hostPid || hostPid == GetCurrentProcessId()) return false;
    std::wstring mappingName = tasked::trayhook::mappingNamePrefix;
    mappingName += std::to_wstring(hostPid);
    HANDLE mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, mappingName.c_str());
    if (!mapping) return false;
    struct MappingGuard { HANDLE value; ~MappingGuard() { CloseHandle(value); } } mappingGuard{mapping};
    void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(SharedRequest));
    if (!view) return false;
    struct ViewGuard { void* value; ~ViewGuard() { UnmapViewOfFile(value); } } viewGuard{view};
    SharedRequest shared{};
    if (!ReadCurrent(view, &shared, sizeof(shared)) || shared.magic != tasked::trayhook::requestMagic || shared.version != tasked::trayhook::protocolVersion || shared.size != sizeof(SharedRequest) || shared.hostPid != hostPid || shared.generation != generation || !shared.targetWindow || !shared.taskbarTimeDateStamp || !shared.taskbarSizeOfImage) return false;
    DWORD targetPid = 0;
    if (!IsWindow(shared.targetWindow) || !GetWindowThreadProcessId(shared.targetWindow, &targetPid) || targetPid != hostPid) return false;
    *result = ScanRequest{hostPid, shared.targetWindow, generation, shared};
    return true;
}

bool QueueScan(const ScanRequest& request) noexcept
{
    try {
        auto dispatcher = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
        if (!dispatcher) return false;
        return dispatcher.TryEnqueue([request]() noexcept {
            try { RunScanAsync(request); }
            catch (...) { InterlockedExchange(&scanActive, 0); }
        });
    } catch (...) { return false; }
}
}

extern "C" __declspec(dllexport) LRESULT CALLBACK TaskedTrayHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && lParam) {
        CWPSTRUCT message{};
        if (ReadCurrent(reinterpret_cast<const void*>(lParam), &message, sizeof(message)) && message.message == ScanMessage() && IsTaskbarWindow(message.hwnd)) {
            ScanRequest request{};
            if (ReadRequest(message.wParam, message.lParam, &request) && InterlockedCompareExchange(&scanActive, 1, 0) == 0 && !QueueScan(request)) InterlockedExchange(&scanActive, 0);
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
