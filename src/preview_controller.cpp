#include "preview_controller.h"

#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

void PreviewController::hide()
{
#ifdef Q_OS_WIN
    if (thumbnail) {
        DwmUnregisterThumbnail(reinterpret_cast<HTHUMBNAIL>(thumbnail));
        thumbnail = 0;
    }
#endif
}

void PreviewController::show(const QString &windowHandle, QObject *destination, int width, int height)
{
#ifdef Q_OS_WIN
    hide();
    auto *destinationWindow = qobject_cast<QWindow *>(destination);
    bool ok = false;
    const auto value = windowHandle.toULongLong(&ok);
    const auto source = reinterpret_cast<HWND>(static_cast<quintptr>(value));
    if (!ok || !destinationWindow || !IsWindow(source)) return;
    const auto target = reinterpret_cast<HWND>(destinationWindow->winId());
    HTHUMBNAIL thumb = nullptr;
    if (FAILED(DwmRegisterThumbnail(target, source, &thumb)) || !thumb) return;
    const auto scale = destinationWindow->devicePixelRatio();
    DWM_THUMBNAIL_PROPERTIES properties{};
    properties.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY;
    properties.rcDestination = { static_cast<LONG>(14 * scale), static_cast<LONG>(30 * scale), static_cast<LONG>((width - 14) * scale), static_cast<LONG>((height - 14) * scale) };
    properties.fVisible = TRUE;
    properties.opacity = 255;
    if (FAILED(DwmUpdateThumbnailProperties(thumb, &properties))) {
        DwmUnregisterThumbnail(thumb);
        return;
    }
    thumbnail = reinterpret_cast<quintptr>(thumb);
#else
    Q_UNUSED(windowHandle);
    Q_UNUSED(destination);
    Q_UNUSED(width);
    Q_UNUSED(height);
#endif
}
