#pragma once
#include <windows.h>

namespace tasked::trayhook {
void startClient(HWND receiverWindow);
void requestScan(HWND taskbarWindow);
void stopClient();
}
