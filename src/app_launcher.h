#pragma once

#include <QObject>

class AppLauncher final : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE bool launch(const QString &target);
};
