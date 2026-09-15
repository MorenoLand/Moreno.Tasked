#include "shell_icon_provider.h"

#include <QImage>
#include <QStandardPaths>
#include <QUrl>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

ShellIconProvider::ShellIconProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}

QPixmap ShellIconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    const auto requested = requestedSize.isValid() ? requestedSize : QSize(48, 48);
    const auto target = QUrl::fromPercentEncoding(id.toUtf8());
    const auto executable = QStandardPaths::findExecutable(target);
    if (size) *size = requested;
#ifdef Q_OS_WIN
    SHFILEINFOW fileInfo{};
    const auto path = executable.isEmpty() ? target : executable;
    if (SHGetFileInfoW(reinterpret_cast<LPCWSTR>(path.utf16()), 0, &fileInfo, sizeof(fileInfo), SHGFI_ICON | SHGFI_LARGEICON) && fileInfo.hIcon) {
        const auto image = QImage::fromHICON(fileInfo.hIcon);
        DestroyIcon(fileInfo.hIcon);
        return QPixmap::fromImage(image.scaled(requested, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
#endif
    return {};
}
