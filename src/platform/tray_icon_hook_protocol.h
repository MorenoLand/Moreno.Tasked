#pragma once
#include <windows.h>
#include <cstdint>

namespace tasked::trayhook {
inline constexpr std::uint32_t requestMagic = 0x544B4851;
inline constexpr std::uint32_t packetMagic = 0x544B4951;
inline constexpr std::uint32_t protocolVersion = 3;
inline constexpr std::uint32_t maxIconSide = 512;
inline constexpr std::uint32_t maxIconPixelBytes = maxIconSide * maxIconSide * 4;
inline constexpr std::uint32_t maxTrayItems = 256;
inline constexpr std::uint32_t maxPacketTextBytes = 1024;
inline constexpr std::uint32_t iconPacketKind = 1;
inline constexpr std::uint32_t scanCompletePacketKind = 2;
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
    std::uint32_t screenWidth;
    std::uint32_t screenHeight;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t stride;
    std::uint32_t pixelBytes;
    std::uint32_t packetKind;
    std::uint32_t expectedPacketCount;
};
#pragma pack(pop)
}
