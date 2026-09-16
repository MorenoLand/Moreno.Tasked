#pragma once

#include <QQuickImageProvider>

class TrayModel;

class TrayIconProvider final : public QQuickImageProvider
{
public:
    explicit TrayIconProvider(TrayModel *model);
    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
private:
    TrayModel *model;
};
