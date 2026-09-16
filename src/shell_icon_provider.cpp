#include "shell_icon_provider.h"

#include <QFileInfo>
#include <QImage>
#include <QStandardPaths>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

#ifdef Q_OS_WIN
QPixmap iconFromHandle(HICON icon, const QSize &requested)
{
    if (!icon) return {};
    const auto copy = CopyIcon(icon);
    const auto image = QImage::fromHICON(copy ? copy : icon);
    if (copy) DestroyIcon(copy);
    return image.isNull() ? QPixmap() : QPixmap::fromImage(image.scaled(requested, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

QPixmap iconFromPath(const QString &path, const QSize &requested)
{
    if (path.isEmpty()) return {};
    SHFILEINFOW fileInfo{};
    if (!SHGetFileInfoW(reinterpret_cast<LPCWSTR>(path.utf16()), 0, &fileInfo, sizeof(fileInfo), SHGFI_ICON | SHGFI_LARGEICON)) return {};
    const auto result = iconFromHandle(fileInfo.hIcon, requested);
    if (fileInfo.hIcon) DestroyIcon(fileInfo.hIcon);
    return result;
}
#endif

ShellIconProvider::ShellIconProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

QPixmap ShellIconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    const auto requested = requestedSize.isValid() ? requestedSize : QSize(48, 48);
    const auto target = QUrl::fromPercentEncoding(id.toUtf8());
    const auto executable = QStandardPaths::findExecutable(target);
    if (size) *size = requested;
#ifdef Q_OS_WIN
    if (target.startsWith(QStringLiteral("window/"))) {
        bool ok = false;
        const auto value = target.mid(7).toULongLong(&ok);
        const auto window = reinterpret_cast<HWND>(static_cast<quintptr>(value));
        if (ok && IsWindow(window)) {
            auto icon = reinterpret_cast<HICON>(SendMessageW(window, WM_GETICON, ICON_BIG, 0));
            if (!icon) icon = reinterpret_cast<HICON>(SendMessageW(window, WM_GETICON, ICON_SMALL2, 0));
            if (!icon) icon = reinterpret_cast<HICON>(SendMessageW(window, WM_GETICON, ICON_SMALL, 0));
            if (!icon) icon = reinterpret_cast<HICON>(GetClassLongPtrW(window, GCLP_HICON));
            if (const auto result = iconFromHandle(icon, requested); !result.isNull()) return result;
        }
    }
    const auto path = QFileInfo::exists(target) ? target : executable;
    return iconFromPath(path, requested);
#endif
    return {};
}
