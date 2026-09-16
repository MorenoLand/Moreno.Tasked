#pragma once

#include <QObject>

class PreviewController final : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE void show(const QString &windowHandle, QObject *destination, int width, int height);
    Q_INVOKABLE void hide();
private:
    quintptr thumbnail = 0;
};
