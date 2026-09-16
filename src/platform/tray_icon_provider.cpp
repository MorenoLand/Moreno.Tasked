#include "tray_icon_provider.h"

#include "windows_shell.h"

TrayIconProvider::TrayIconProvider(TrayModel *model) : QQuickImageProvider(QQuickImageProvider::Pixmap), model(model) {}

QPixmap TrayIconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    bool ok = false;
    const auto key = id.toULongLong(&ok);
    const auto requested = requestedSize.isValid() ? requestedSize : QSize(32, 32);
    if (size) *size = requested;
    return ok && model ? model->icon(key, requested) : QPixmap();
}
