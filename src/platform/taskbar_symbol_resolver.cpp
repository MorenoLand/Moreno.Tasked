#include "taskbar_symbol_resolver.h"
#include <dbghelp.h>
#include <winhttp.h>
#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace tasked::trayhook {
namespace {
constexpr DWORD symbolOptions = SYMOPT_UNDNAME | SYMOPT_NO_PROMPTS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_EXACT_SYMBOLS;
constexpr wchar_t symbolCacheDirectory[] = L"symbols";
constexpr size_t pathCapacity = 32768;
std::mutex resolverMutex;

struct ImageIdentity {
    DWORD timeDateStamp = 0;
    DWORD sizeOfImage = 0;
    GUID pdbGuid{};
    DWORD pdbAge = 0;
    std::wstring pdbFileName;
};

struct RequiredSymbol {
    std::wstring_view name;
    DWORD64 address = 0;
    size_t matches = 0;
};

struct SymbolEnumeration {
    std::array<RequiredSymbol, 3> symbols;
};

class SymbolSession {
public:
    explicit SymbolSession(const std::wstring& searchPath) : process_(GetCurrentProcess()), previousOptions_(SymGetOptions()) {
        SymSetOptions(previousOptions_ | symbolOptions);
        initialized_ = SymInitializeW(process_, searchPath.c_str(), FALSE) != FALSE;
    }
    ~SymbolSession() { close(); }
    bool initialized() const { return initialized_; }
    HANDLE process() const { return process_; }
    bool close() {
        if (closed_) return cleanupResult_;
        cleanupResult_ = !initialized_ || SymCleanup(process_) != FALSE;
        initialized_ = false;
        SymSetOptions(previousOptions_);
        closed_ = true;
        return cleanupResult_;
    }
private:
    HANDLE process_;
    DWORD previousOptions_;
    bool initialized_ = false;
    bool closed_ = false;
    bool cleanupResult_ = false;
};

class LoadedModule {
public:
    explicit LoadedModule(HMODULE module = nullptr) : module_(module) {}
    ~LoadedModule() { if (module_) FreeLibrary(module_); }
    HMODULE get() const { return module_; }
private:
    HMODULE module_;
};

bool getCurrentExecutableDirectory(std::wstring& directory) {
    std::wstring path(pathCapacity, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return false;
    path.resize(length);
    const size_t separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) return false;
    directory = path.substr(0, separator);
    const size_t parentSeparator = directory.find_last_of(L"\\/");
    if (parentSeparator == std::wstring::npos) return false;
    const std::wstring_view leaf(directory.data() + parentSeparator + 1, directory.size() - parentSeparator - 1);
    return CompareStringOrdinal(leaf.data(), static_cast<int>(leaf.size()), L"bin", 3, TRUE) == CSTR_EQUAL;
}

bool getSymbolCachePath(std::wstring& cachePath, std::wstring& searchPath) {
    std::wstring executableDirectory;
    if (!getCurrentExecutableDirectory(executableDirectory) || executableDirectory.find(L';') != std::wstring::npos) return false;
    cachePath = executableDirectory + L"\\" + symbolCacheDirectory;
    if (!CreateDirectoryW(cachePath.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    const DWORD attributes = GetFileAttributesW(cachePath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY) || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    searchPath = cachePath;
    return true;
}

std::wstring makePdbSignature(const ImageIdentity& identity) {
    std::wostringstream value;
    value << std::uppercase << std::hex << std::setfill(L'0') << std::setw(8) << identity.pdbGuid.Data1 << std::setw(4) << identity.pdbGuid.Data2 << std::setw(4) << identity.pdbGuid.Data3;
    for (const auto byte : identity.pdbGuid.Data4) value << std::setw(2) << static_cast<unsigned>(byte);
    value << std::setw(1) << identity.pdbAge;
    return value.str();
}

struct WinHttpHandle { explicit WinHttpHandle(HINTERNET handle) : value(handle) {} WinHttpHandle(const WinHttpHandle&) = delete; WinHttpHandle& operator=(const WinHttpHandle&) = delete; ~WinHttpHandle() { if (value) WinHttpCloseHandle(value); } HINTERNET value = nullptr; };

bool fetchPdb(const ImageIdentity& identity, const std::wstring& pdbPath) {
    const auto session = WinHttpHandle{WinHttpOpen(L"Tasked", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (!session.value) return false;
    WinHttpSetTimeouts(session.value, 3000, 3000, 5000, 5000);
    const auto connection = WinHttpHandle{WinHttpConnect(session.value, L"msdl.microsoft.com", INTERNET_DEFAULT_HTTPS_PORT, 0)};
    if (!connection.value) return false;
    const auto signature = makePdbSignature(identity);
    const auto requestPath = L"/download/symbols/" + identity.pdbFileName + L"/" + signature + L"/" + identity.pdbFileName;
    const auto request = WinHttpHandle{WinHttpOpenRequest(connection.value, L"GET", requestPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)};
    if (!request.value || !WinHttpSendRequest(request.value, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request.value, nullptr)) return false;
    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    if (!WinHttpQueryHeaders(request.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX) || status != HTTP_STATUS_OK) return false;
    const auto temporaryPath = pdbPath + L".download";
    const auto file = CreateFileW(temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    bool success = true;
    std::uint64_t totalBytes = 0;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.value, &available)) { success = false; break; }
        if (!available) break;
        if (totalBytes + available > 128ull * 1024 * 1024) { success = false; break; }
        std::vector<std::uint8_t> buffer(available);
        DWORD received = 0;
        if (!WinHttpReadData(request.value, buffer.data(), available, &received) || !received) { success = false; break; }
        DWORD written = 0;
        if (!WriteFile(file, buffer.data(), received, &written, nullptr) || written != received) { success = false; break; }
        totalBytes += received;
    }
    if (success && totalBytes) success = FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    if (!success || !totalBytes || !MoveFileExW(temporaryPath.c_str(), pdbPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { DeleteFileW(temporaryPath.c_str()); return false; }
    return true;
}

bool preparePdbCache(const ImageIdentity& identity, const std::wstring& cachePath, std::wstring& pdbSearchPath) {
    if (identity.pdbFileName.empty() || identity.pdbFileName.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos) return false;
    const auto pdbDirectory = cachePath + L"\\" + identity.pdbFileName + L"\\" + makePdbSignature(identity);
    if (!CreateDirectoryW((cachePath + L"\\" + identity.pdbFileName).c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    if (!CreateDirectoryW(pdbDirectory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    const auto attributes = GetFileAttributesW(pdbDirectory.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY) || (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    const auto pdbPath = pdbDirectory + L"\\" + identity.pdbFileName;
    const auto existing = GetFileAttributesW(pdbPath.c_str());
    if (existing == INVALID_FILE_ATTRIBUTES && !fetchPdb(identity, pdbPath)) return false;
    pdbSearchPath = pdbDirectory;
    return true;
}

bool getSystemTaskbarPath(std::wstring& path) {
    std::wstring directory(pathCapacity, L'\0');
    const UINT length = GetSystemDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
    if (!length || length >= directory.size()) return false;
    directory.resize(length);
    path = directory + L"\\taskbar.dll";
    return true;
}

bool getLoadedModulePath(HMODULE module, std::wstring& path) {
    path.assign(pathCapacity, L'\0');
    const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return false;
    path.resize(length);
    return true;
}

bool pathEquals(std::wstring_view left, std::wstring_view right) {
    return left.size() <= INT_MAX && right.size() <= INT_MAX && CompareStringOrdinal(left.data(), static_cast<int>(left.size()), right.data(), static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

bool pathIsWithin(std::wstring_view path, std::wstring_view directory) {
    if (path.size() <= directory.size() || path.size() > INT_MAX || directory.size() > INT_MAX) return false;
    if (CompareStringOrdinal(path.data(), static_cast<int>(directory.size()), directory.data(), static_cast<int>(directory.size()), TRUE) != CSTR_EQUAL) return false;
    return path[directory.size()] == L'\\' || path[directory.size()] == L'/';
}

bool imageRange(DWORD imageSize, DWORD rva, size_t size) {
    return rva < imageSize && size <= static_cast<size_t>(imageSize - rva);
}

bool readImageIdentity(HMODULE module, ImageIdentity& identity) {
    const auto* base = reinterpret_cast<const std::byte*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
#if defined(_M_X64)
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64) return false;
#elif defined(_M_ARM64)
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_ARM64) return false;
#else
    return false;
#endif
    const DWORD imageSize = nt->OptionalHeader.SizeOfImage;
    const auto& directories = nt->OptionalHeader.DataDirectory;
    if (!imageSize || nt->FileHeader.TimeDateStamp == 0 || IMAGE_DIRECTORY_ENTRY_DEBUG >= nt->OptionalHeader.NumberOfRvaAndSizes) return false;
    const auto& debugData = directories[IMAGE_DIRECTORY_ENTRY_DEBUG];
    if (!debugData.VirtualAddress || !debugData.Size || debugData.Size % sizeof(IMAGE_DEBUG_DIRECTORY) || !imageRange(imageSize, debugData.VirtualAddress, debugData.Size)) return false;
    const auto* entries = reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(base + debugData.VirtualAddress);
    const size_t entryCount = debugData.Size / sizeof(IMAGE_DEBUG_DIRECTORY);
    bool foundCodeView = false;
    GUID guid{};
    DWORD age = 0;
    std::wstring pdbFileName;
    for (size_t i = 0; i < entryCount; ++i) {
        if (entries[i].Type != IMAGE_DEBUG_TYPE_CODEVIEW) continue;
        constexpr size_t codeViewHeaderSize = sizeof(DWORD) + sizeof(GUID) + sizeof(DWORD);
        if (foundCodeView || entries[i].SizeOfData < codeViewHeaderSize + 1 || !imageRange(imageSize, entries[i].AddressOfRawData, entries[i].SizeOfData)) return false;
        const auto* record = reinterpret_cast<const std::byte*>(base + entries[i].AddressOfRawData);
        if (std::memcmp(record, "RSDS", 4) != 0) return false;
        std::memcpy(&guid, record + sizeof(DWORD), sizeof(guid));
        std::memcpy(&age, record + sizeof(DWORD) + sizeof(guid), sizeof(age));
        const auto *pathStart = reinterpret_cast<const char *>(record + codeViewHeaderSize);
        const auto *pathEnd = static_cast<const char *>(std::memchr(pathStart, 0, entries[i].SizeOfData - codeViewHeaderSize));
        if (!pathEnd || !age) return false;
        std::string_view fileName(pathStart, static_cast<size_t>(pathEnd - pathStart));
        const auto separator = fileName.find_last_of("/\\");
        if (separator != std::string_view::npos) fileName.remove_prefix(separator + 1);
        if (fileName.empty() || fileName.size() > 128 || !std::all_of(fileName.begin(), fileName.end(), [](unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'; })) return false;
        pdbFileName.assign(fileName.begin(), fileName.end());
        foundCodeView = true;
    }
    if (!foundCodeView) return false;
    identity.timeDateStamp = nt->FileHeader.TimeDateStamp;
    identity.sizeOfImage = imageSize;
    identity.pdbGuid = guid;
    identity.pdbAge = age;
    identity.pdbFileName = std::move(pdbFileName);
    return true;
}

bool pdbIdentityMatches(HANDLE process, DWORD64 moduleBase, const ImageIdentity& identity, std::wstring_view cachePath) {
    IMAGEHLP_MODULEW64 moduleInfo{};
    moduleInfo.SizeOfStruct = sizeof(moduleInfo);
    if (!SymGetModuleInfoW64(process, moduleBase, &moduleInfo) || moduleInfo.SymType != SymPdb || moduleInfo.PdbUnmatched || moduleInfo.BaseOfImage != moduleBase || moduleInfo.ImageSize != identity.sizeOfImage || moduleInfo.TimeDateStamp != identity.timeDateStamp || moduleInfo.PdbAge != identity.pdbAge || std::memcmp(&moduleInfo.PdbSig70, &identity.pdbGuid, sizeof(GUID)) != 0 || !moduleInfo.LoadedPdbName[0]) return false;
    std::wstring loadedPdb(moduleInfo.LoadedPdbName);
    std::replace(loadedPdb.begin(), loadedPdb.end(), L'/', L'\\');
    std::wstring cache(cachePath);
    std::replace(cache.begin(), cache.end(), L'/', L'\\');
    while (cache.size() > 3 && cache.back() == L'\\') cache.pop_back();
    return pathIsWithin(loadedPdb, cache);
}

std::wstring_view trimTrailingSpaces(std::wstring_view value) {
    while (!value.empty() && (value.back() == L' ' || value.back() == L'\t')) value.remove_suffix(1);
    return value;
}

BOOL CALLBACK collectSymbols(PSYMBOL_INFOW symbol, ULONG, PVOID userData) {
    auto& enumeration = *static_cast<SymbolEnumeration*>(userData);
    size_t length = symbol->NameLen;
    if (length && symbol->Name[length - 1] == L'\0') --length;
    const std::wstring_view name = trimTrailingSpaces(std::wstring_view(symbol->Name, length));
    for (auto& required : enumeration.symbols) {
        if (name == required.name) {
            ++required.matches;
            required.address = symbol->Address;
        }
    }
    return TRUE;
}

bool isImageAddress(DWORD64 moduleBase, DWORD imageSize, DWORD64 address, size_t size) {
    return address >= moduleBase && address - moduleBase < imageSize && size <= static_cast<size_t>(imageSize - (address - moduleBase));
}

bool isExecutableAddress(HMODULE module, DWORD64 address, size_t size) {
    MEMORY_BASIC_INFORMATION memory{};
    if (!VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) || memory.AllocationBase != module) return false;
    const DWORD64 regionOffset = address - reinterpret_cast<DWORD64>(memory.BaseAddress);
    if (regionOffset > memory.RegionSize || size > memory.RegionSize - regionOffset) return false;
    const DWORD protection = memory.Protect & 0xff;
    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

bool validateFrameHeightPrologue(const std::byte* prologue, size_t available, uint64_t& elementOffset) {
#if defined(_M_X64)
    if (available < 8) return false;
    const auto* bytes = reinterpret_cast<const unsigned char*>(prologue);
    if (bytes[0] != 0x48 || bytes[1] != 0x83 || bytes[2] != 0xEC || bytes[4] != 0x48 || bytes[5] != 0x83 || bytes[6] != 0xC1 || bytes[7] == 0 || bytes[7] > 0x7f || bytes[7] % sizeof(void*) != 0) return false;
    elementOffset = bytes[7];
    return true;
#elif defined(_M_ARM64)
    if (available < 16) return false;
    std::array<DWORD, 4> instructions{};
    std::memcpy(instructions.data(), prologue, sizeof(instructions));
    if (instructions[0] != 0xD503237F || (instructions[1] & 0xFFC07FFF) != 0xA9807BFD || instructions[2] != 0x910003FD || (instructions[3] & 0xFFF00FE0) != 0xF8400C00) return false;
    elementOffset = (instructions[3] >> 12) & 0xff;
    return elementOffset && elementOffset <= 0x7f && elementOffset % sizeof(void*) == 0;
#else
    return false;
#endif
}
}

bool resolveTaskbarSymbols(SharedRequest& request) {
    std::lock_guard lock(resolverMutex);
    std::wstring cachePath;
    std::wstring searchPath;
    if (!getSymbolCachePath(cachePath, searchPath)) return false;
    std::wstring taskbarPath;
    if (!getSystemTaskbarPath(taskbarPath)) return false;
    const HMODULE loaded = LoadLibraryExW(taskbarPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!loaded) return false;
    LoadedModule module(loaded);
    std::wstring actualModulePath;
    if (!getLoadedModulePath(loaded, actualModulePath) || !pathEquals(actualModulePath, taskbarPath)) return false;
    ImageIdentity identity;
    if (!readImageIdentity(loaded, identity)) return false;
    if (!preparePdbCache(identity, cachePath, searchPath)) return false;
    SymbolSession symbols(searchPath);
    if (!symbols.initialized()) return false;
    const DWORD64 requestedBase = reinterpret_cast<DWORD64>(loaded);
    const DWORD64 moduleBase = SymLoadModuleExW(symbols.process(), nullptr, taskbarPath.c_str(), nullptr, requestedBase, identity.sizeOfImage, nullptr, 0);
    if (!moduleBase || moduleBase != requestedBase) return false;
    SymbolEnumeration enumeration{};
    enumeration.symbols[0].name = L"CTaskBand::GetTaskbarHost";
    enumeration.symbols[1].name = L"TaskbarHost::FrameHeight";
    enumeration.symbols[2].name = L"std::_Ref_count_base::_Decref";
    if (!SymEnumSymbolsW(symbols.process(), moduleBase, L"*", collectSymbols, &enumeration) || !pdbIdentityMatches(symbols.process(), moduleBase, identity, cachePath)) return false;
    for (const auto& symbol : enumeration.symbols) if (symbol.matches != 1 || !isImageAddress(moduleBase, identity.sizeOfImage, symbol.address, 1)) return false;
    const DWORD64 frameHeightAddress = enumeration.symbols[1].address;
    if (!isImageAddress(moduleBase, identity.sizeOfImage, frameHeightAddress, 16) || !isExecutableAddress(loaded, frameHeightAddress, 16)) return false;
    uint64_t frameHeightOffset = 0;
    if (!validateFrameHeightPrologue(reinterpret_cast<const std::byte*>(frameHeightAddress), 16, frameHeightOffset)) return false;
    SharedRequest resolved = request;
    resolved.magic = requestMagic;
    resolved.version = protocolVersion;
    resolved.size = sizeof(SharedRequest);
    resolved.taskbarTimeDateStamp = identity.timeDateStamp;
    resolved.taskbarSizeOfImage = identity.sizeOfImage;
    resolved.getTaskbarHostRva = enumeration.symbols[0].address - moduleBase;
    resolved.frameHeightRva = enumeration.symbols[1].address - moduleBase;
    resolved.refCountDecrefRva = enumeration.symbols[2].address - moduleBase;
    if (!symbols.close()) return false;
    request = resolved;
    return true;
}
}
