#pragma once
#include <windows.h>
#include <cstdint>

namespace tasked::trayhook {
inline constexpr std::uint32_t requestMagic = 0x544B4851;
inline constexpr std::uint32_t packetMagic = 0x544B4951;
inline constexpr std::uint32_t protocolVersion = 1;
inline constexpr ULONG_PTR copyDataTag = 0x544B4951;
inline constexpr wchar_t mappingNamePrefix[] = L"Local\\TaskedTrayHook_";
inline UINT scanMessage() { static const auto message = RegisterWindowMessageW(L"Tasked.TrayHook.Scan.v1"); return message; }

struct SharedRequest {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t size;
    std::uint32_t generation;
    DWORD hostPid;
    HWND targetWindow;
    std::uint32_t taskbarTimeDateStamp;
    std::uint32_t taskbarSizeOfImage;
    std::uint64_t getTaskbarHostRva;
    std::uint64_t frameHeightRva;
    std::uint64_t refCountDecrefRva;
};

#pragma pack(push, 8)
struct IconPacketHeader {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint64_t requestId;
    std::uint32_t automationIdLength;
    std::uint32_t nameLength;
    std::uint32_t ordinal;
    std::int32_t screenX;
    std::int32_t screenY;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t stride;
    std::uint32_t pixelBytes;
};
#pragma pack(pop)
}
