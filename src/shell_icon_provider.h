#pragma once

#include <QQuickImageProvider>

class ShellIconProvider final : public QQuickImageProvider
{
public:
    ShellIconProvider();
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
};
