#include "tray_icon_hook_client.h"
#include "taskbar_symbol_resolver.h"
#include "tray_icon_hook_protocol.h"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace tasked::trayhook {
namespace {
struct State {
    std::mutex mutex;
    std::condition_variable stopped;
    std::atomic<std::uint32_t> generation{0};
    HWND receiver = nullptr;
    HWND taskbar = nullptr;
    HMODULE hookModule = nullptr;
    HHOOK hook = nullptr;
    HANDLE mapping = nullptr;
    SharedRequest *request = nullptr;
    std::uint64_t lastRequest = 0;
    std::uint64_t lastAttempt = 0;
    bool starting = false;
    bool ready = false;
    bool stopping = false;
};

State &state() { static auto *value = new State; return *value; }

void releaseResources(HHOOK hook, HANDLE mapping, SharedRequest *request, HMODULE module) {
    if (hook) UnhookWindowsHookEx(hook);
    if (request) UnmapViewOfFile(request);
    if (mapping) CloseHandle(mapping);
    if (module) FreeLibrary(module);
}

void initialize(HWND taskbar, HWND receiver, std::uint32_t generation) {
    const auto pid = GetCurrentProcessId();
    auto mappingName = std::wstring(mappingNamePrefix) + std::to_wstring(pid);
    const auto mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(SharedRequest), mappingName.c_str());
    if (!mapping) { auto &current = state(); std::lock_guard lock(current.mutex); if (current.generation.load() == generation) current.starting = false; return; }
    auto *request = static_cast<SharedRequest *>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedRequest)));
    if (!request) { CloseHandle(mapping); auto &current = state(); std::lock_guard lock(current.mutex); if (current.generation.load() == generation) current.starting = false; return; }
    ZeroMemory(request, sizeof(*request));
    request->magic = requestMagic;
    request->version = protocolVersion;
    request->size = sizeof(*request);
    request->generation = generation;
    request->hostPid = pid;
    request->targetWindow = receiver;
    if (!resolveTaskbarSymbols(*request)) { UnmapViewOfFile(request); CloseHandle(mapping); auto &current = state(); std::lock_guard lock(current.mutex); if (current.generation.load() == generation) current.starting = false; return; }
    wchar_t executable[MAX_PATH]{};
    const auto executableLength = GetModuleFileNameW(nullptr, executable, ARRAYSIZE(executable));
    if (!executableLength || executableLength >= ARRAYSIZE(executable)) { UnmapViewOfFile(request); CloseHandle(mapping); auto &current = state(); std::lock_guard lock(current.mutex); if (current.generation.load() == generation) current.starting = false; return; }
    const auto hookPath = std::filesystem::path(executable).parent_path() / L"TaskedTrayHook.dll";
    const auto module = LoadLibraryExW(hookPath.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!module) { UnmapViewOfFile(request); CloseHandle(mapping); auto &current = state(); std::lock_guard lock(current.mutex); if (current.generation.load() == generation) current.starting = false; return; }
    const auto proc = reinterpret_cast<HOOKPROC>(GetProcAddress(module, "TaskedTrayHookProc"));
    if (!proc) { FreeLibrary(module); UnmapViewOfFile(request); CloseHandle(mapping); auto &current = state(); std::lock_guard lock(current.mutex); if (current.generation.load() == generation) current.starting = false; return; }
    const auto threadId = GetWindowThreadProcessId(taskbar, nullptr);
    const auto hook = threadId ? SetWindowsHookExW(WH_CALLWNDPROC, proc, module, threadId) : nullptr;
    if (!hook) { FreeLibrary(module); UnmapViewOfFile(request); CloseHandle(mapping); auto &current = state(); std::lock_guard lock(current.mutex); if (current.generation.load() == generation) current.starting = false; return; }
    bool stale = false;
    {
        auto &current = state();
        std::lock_guard lock(current.mutex);
        if (current.stopping || current.generation.load() != generation || current.taskbar != taskbar) {
            stale = true;
            current.starting = false;
        } else {
            current.hook = hook;
            current.mapping = mapping;
            current.request = request;
            current.hookModule = module;
            current.ready = true;
            current.starting = false;
        }
    }
    if (stale) releaseResources(hook, mapping, request, module);
    else { auto &current = state(); std::unique_lock lock(current.mutex); current.stopped.wait(lock, [&] { return current.stopping || current.generation.load() != generation; }); }
}

void begin(HWND taskbar, HWND receiver) {
    auto &current = state();
    std::uint32_t generation = 0;
    HHOOK oldHook = nullptr;
    HANDLE oldMapping = nullptr;
    SharedRequest *oldRequest = nullptr;
    HMODULE oldModule = nullptr;
    {
        std::lock_guard lock(current.mutex);
        if (current.stopping || !taskbar || !receiver) return;
        if (current.taskbar == taskbar && (current.ready || current.starting)) return;
        const auto now = GetTickCount64();
        if (now - current.lastAttempt < 10000) return;
        current.lastAttempt = now;
        current.generation.fetch_add(1);
        current.stopped.notify_all();
        generation = current.generation.load();
        current.taskbar = taskbar;
        current.receiver = receiver;
        current.lastRequest = 0;
        current.starting = true;
        current.ready = false;
        oldHook = std::exchange(current.hook, nullptr);
        oldMapping = std::exchange(current.mapping, nullptr);
        oldRequest = std::exchange(current.request, nullptr);
        oldModule = std::exchange(current.hookModule, nullptr);
    }
    releaseResources(oldHook, oldMapping, oldRequest, oldModule);
    std::thread(initialize, taskbar, receiver, generation).detach();
}
}

void startClient(HWND receiverWindow) { auto &current = state(); std::lock_guard lock(current.mutex); current.receiver = receiverWindow; current.stopping = false; }

void requestScan(HWND taskbarWindow) {
    auto &current = state();
    HWND receiver = nullptr;
    { std::lock_guard lock(current.mutex); receiver = current.receiver; }
    begin(taskbarWindow, receiver);
    std::lock_guard lock(current.mutex);
    if (!current.ready || !current.request || !IsWindow(current.taskbar)) return;
    const auto now = GetTickCount64();
    if (now - current.lastRequest < 3000) return;
    current.lastRequest = now;
    const auto generation = current.generation.fetch_add(1) + 1;
    InterlockedExchange(reinterpret_cast<volatile LONG *>(&current.request->generation), static_cast<LONG>(generation));
    DWORD_PTR response = 0;
    SendMessageTimeoutW(current.taskbar, tasked::trayhook::scanMessage(), GetCurrentProcessId(), generation, SMTO_ABORTIFHUNG | SMTO_BLOCK, 1000, &response);
}

void stopClient() {
    auto &current = state();
    HHOOK hook = nullptr;
    HANDLE mapping = nullptr;
    SharedRequest *request = nullptr;
    HMODULE module = nullptr;
    {
        std::lock_guard lock(current.mutex);
        current.stopping = true;
        current.generation.fetch_add(1);
        current.stopped.notify_all();
        current.starting = false;
        current.ready = false;
        hook = std::exchange(current.hook, nullptr);
        mapping = std::exchange(current.mapping, nullptr);
        request = std::exchange(current.request, nullptr);
        module = std::exchange(current.hookModule, nullptr);
    }
    releaseResources(hook, mapping, request, module);
}
}
