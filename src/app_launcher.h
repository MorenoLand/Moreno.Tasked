#pragma once

#include <QObject>

class AppLauncher final : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE bool launch(const QString &target);
    Q_INVOKABLE void showStartMenu(int x, int y, int width, int height);
    Q_INVOKABLE void showSystemTrayFlyout(int x, int y, int width, int height);
};
